/*
 * Copyright (c) 1999-2009 Apple Inc. All rights reserved.
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
 * 
 * June 16, 2003	Dieter Siegmund (dieter@apple.com)
 * - added support for firewire (IFT_IEEE1394)
 *
 * November 19, 2003	Dieter Siegmund (dieter@apple.com)
 * - added support for VLAN's (IFT_L2VLAN)
 *
 * April 13, 2005	Dieter Siegmund (dieter@apple.com)
 * - added support for multiple link-local services, one per interface
 * - changed logic to favor a successful service (i.e. one with an IP address)
 *   over simply the highest ranked service per interface
 * - maintain the link-local subnet on the interface with the highest ranked
 *   active service
 *
 * October 20, 2006	Dieter Siegmund (dieter@apple.com)
 * - resolve the router's MAC address using ARP, and publish that
 *   information to the NetworkSignature in the IPv4 dict
 *
 * December 19, 2007	Dieter Siegmund (dieter@apple.com)
 * - removed subnet route maintenance code since IPMonitor now takes
 *   care of that
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
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
#include <net/firewire.h>
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
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOHibernatePrivate.h>
#include <TargetConditionals.h>
#if ! TARGET_OS_EMBEDDED
#include <CoreFoundation/CFUserNotification.h>
#include <CoreFoundation/CFUserNotificationPriv.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#endif /* ! TARGET_OS_EMBEDDED */

#include "rfc_options.h"
#include "dhcp_options.h"
#include "interfaces.h"
#include "util.h"
#include "arp.h"

#include "host_identifier.h"
#include "dhcplib.h"
#include "ioregpath.h"

#include "ipconfig_types.h"
#include "ipconfigd.h"
#include "server.h"
#include "timer.h"

#include "ipconfigd_globals.h"
#include "ipconfigd_threads.h"
#include "FDSet.h"
#include "DNSNameList.h"

#include "dprintf.h"

#include "cfutil.h"

typedef dynarray_t	IFStateList_t;

#ifndef IFT_IEEE8023ADLAG
#define IFT_IEEE8023ADLAG 0x88		/* IEEE802.3ad Link Aggregate */
#endif IFT_IEEE8023ADLAG

#ifndef kSCEntNetRefreshConfiguration
#define kSCEntNetRefreshConfiguration	CFSTR("RefreshConfiguration")
#endif kSCEntNetRefreshConfiguration

#ifndef kSCEntNetIPv4ARPCollision
#define kSCEntNetIPv4ARPCollision	CFSTR("IPv4ARPCollision")
#endif kSCEntNetIPv4ARPCollision

#ifndef kSCPropNetLinkDetaching
#define kSCPropNetLinkDetaching		CFSTR("Detaching")	/* CFBoolean */
#endif kSCPropNetLinkDetaching

#ifndef kSCPropNetOverridePrimary
#define kSCPropNetOverridePrimary	CFSTR("OverridePrimary")
#endif kSCPropNetOverridePrimary

#ifndef kSCValNetInterfaceTypeFireWire
#define kSCValNetInterfaceTypeFireWire	CFSTR("FireWire")
#endif kSCValNetInterfaceTypeFireWire

#ifndef kSCValNetIPv4ConfigMethodFailover
#define kSCValNetIPv4ConfigMethodFailover	CFSTR("Failover")
#endif kSCValNetIPv4ConfigMethodFailover

#ifndef kSCPropNetIgnoreLinkStatus
#define kSCPropNetIgnoreLinkStatus	CFSTR("IgnoreLinkStatus")
#endif kSCPropNetIgnoreLinkStatus

#define kDHCPClientPreferencesID	CFSTR("DHCPClient.xml")
#define kDHCPClientApplicationPref	CFSTR("Application")
#define kDHCPRequestedParameterList	CFSTR("DHCPRequestedParameterList")

#ifndef kSCEntNetDHCP
#define kSCEntNetDHCP			CFSTR("DHCP")
#endif kSCEntNetDHCP

#ifndef kSCValNetIPv4ConfigMethodLinkLocal
#define kSCValNetIPv4ConfigMethodLinkLocal	CFSTR("LinkLocal")
#endif kSCValNetIPv4ConfigMethodLinkLocal

/* default values */
#define MAX_RETRIES				9
#define INITIAL_WAIT_SECS			1
#define MAX_WAIT_SECS				8
#define GATHER_SECS				1
#define LINK_INACTIVE_WAIT_SECS			1
#define DHCP_INIT_REBOOT_RETRY_COUNT		2
#define DHCP_SELECT_RETRY_COUNT			3
#define DHCP_ALLOCATE_LINKLOCAL_AT_RETRY_COUNT	4
#define DHCP_ROUTER_ARP_AT_RETRY_COUNT		7
#define DHCP_FAILURE_CONFIGURES_LINKLOCAL	TRUE
#define DHCP_SUCCESS_DECONFIGURES_LINKLOCAL	TRUE
#define DHCP_LOCAL_HOSTNAME_LENGTH_MAX		15
#define DISCOVER_ROUTER_MAC_ADDRESS_SECS	60
#define DHCP_DEFEND_IP_ADDRESS_INTERVAL_SECS	10
#define DHCP_DEFEND_IP_ADDRESS_COUNT		3
#define DHCP_LEASE_WRITE_T1_THRESHOLD_SECS	3600
#define MANUAL_CONFLICT_RETRY_INTERVAL_SECS	300		

#define USER_ERROR			1
#define UNEXPECTED_ERROR 		2
#define TIMEOUT_ERROR			3

/* global variables */
uint16_t 			G_client_port = IPPORT_BOOTPC;
boolean_t 			G_dhcp_accepts_bootp = FALSE;
boolean_t			G_dhcp_failure_configures_linklocal 
				    = DHCP_FAILURE_CONFIGURES_LINKLOCAL;
boolean_t			G_dhcp_success_deconfigures_linklocal 
				    = DHCP_SUCCESS_DECONFIGURES_LINKLOCAL;
int				G_dhcp_allocate_linklocal_at_retry_count 
				    = DHCP_ALLOCATE_LINKLOCAL_AT_RETRY_COUNT;
int				G_dhcp_router_arp_at_retry_count 
				    = DHCP_ROUTER_ARP_AT_RETRY_COUNT;
int				G_dhcp_init_reboot_retry_count 
				    = DHCP_INIT_REBOOT_RETRY_COUNT;
int				G_dhcp_select_retry_count 
				    = DHCP_SELECT_RETRY_COUNT;
int				G_dhcp_defend_ip_address_interval_secs 
				    = DHCP_DEFEND_IP_ADDRESS_INTERVAL_SECS;
int				G_dhcp_defend_ip_address_count
				    = DHCP_DEFEND_IP_ADDRESS_COUNT;
int				G_dhcp_lease_write_t1_threshold_secs
				    = DHCP_LEASE_WRITE_T1_THRESHOLD_SECS;
uint16_t 			G_server_port = IPPORT_BOOTPS;
int				G_manual_conflict_retry_interval_secs
				    = MANUAL_CONFLICT_RETRY_INTERVAL_SECS;

/* 
 * Global: G_link_inactive_secs
 * Purpose:
 *   Time to wait after the link goes inactive before unpublishing 
 *   the interface state information
 */
int				G_link_inactive_secs = LINK_INACTIVE_WAIT_SECS;

/*
 * Global: G_gather_secs
 * Purpose:
 *   Time to wait for the ideal packet after receiving 
 *   the first acceptable packet.
 */ 
int				G_gather_secs = GATHER_SECS;

/*
 * Global: G_initial_wait_secs
 * Purpose:
 *   First timeout interval in seconds.
 */ 
int				G_initial_wait_secs = INITIAL_WAIT_SECS;

/*
 * Global: G_max_retries
 * Purpose:
 *   Number of times to retry sending the packet.
 */ 
int				G_max_retries = MAX_RETRIES;

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
int				G_max_wait_secs = MAX_WAIT_SECS;

boolean_t 			G_must_broadcast = FALSE;
int				G_IPConfiguration_verbose = FALSE;
int				G_debug = FALSE;
bootp_session_t *		G_bootp_session = NULL;
arp_session_t * 		G_arp_session = NULL;
boolean_t			G_router_arp = TRUE;
boolean_t			G_discover_and_publish_router_mac_address = TRUE;

const unsigned char		G_rfc_magic[4] = RFC_OPTIONS_MAGIC;
const struct sockaddr		G_blank_sin = { sizeof(G_blank_sin), AF_INET };
const struct in_addr		G_ip_broadcast = { INADDR_BROADCAST };
const struct in_addr		G_ip_zeroes = { 0 };

/* local variables */
static CFBundleRef		S_bundle = NULL;
static CFRunLoopObserverRef	S_observer = NULL;
static boolean_t		S_linklocal_needs_attention = FALSE;
static IFStateList_t		S_ifstate_list;
static interface_list_t	*	S_interfaces = NULL;
static io_connect_t 		S_power_connection;
static FDSet_t *		S_readers = NULL;
static SCDynamicStoreRef	S_scd_session = NULL;
static CFStringRef		S_setup_service_prefix = NULL;
static CFStringRef		S_state_interface_prefix = NULL;
static char * 			S_computer_name = NULL;
static CFStringRef		S_computer_name_key = NULL;
static CFStringRef		S_hostnames_key = NULL;
static CFStringRef		S_dhcp_preferences_key = NULL;
static int			S_arp_probe_count = ARP_PROBE_COUNT;
static int			S_arp_gratuitous_count = ARP_GRATUITOUS_COUNT;
static struct timeval		S_arp_retry = { 
  ARP_RETRY_SECS,
  ARP_RETRY_USECS
};
static int			S_arp_detect_count = ARP_DETECT_COUNT;
static struct timeval		S_arp_detect_retry = {
    ARP_DETECT_RETRY_SECS,
    ARP_DETECT_RETRY_USECS
};
static int			S_discover_router_mac_address_secs
					= DISCOVER_ROUTER_MAC_ADDRESS_SECS;

static int			S_arp_conflict_retry = ARP_CONFLICT_RETRY_COUNT;
static struct timeval		S_arp_conflict_delay = {
    ARP_CONFLICT_RETRY_DELAY_SECS,
    ARP_CONFLICT_RETRY_DELAY_USECS
};

static int S_dhcp_local_hostname_length_max = DHCP_LOCAL_HOSTNAME_LENGTH_MAX;
static CFArrayRef		S_router_arp_excluded_ssids;
static CFRange			S_router_arp_excluded_ssids_range;


static FILE *	S_IPConfiguration_log_file;
#define IPCONFIGURATION_BOOTP_LOG "/var/log/com.apple.IPConfiguration.bootp"


static struct in_addr		S_netboot_ip;
static struct in_addr		S_netboot_server_ip;
static char			S_netboot_ifname[IFNAMSIZ + 1];

#if ! TARGET_OS_EMBEDDED
static boolean_t		S_use_maintenance_wake = TRUE;
static boolean_t		S_awake = TRUE;
#endif /* ! TARGET_OS_EMBEDDED */

#define PROP_SERVICEID		CFSTR("ServiceID")

/* 
 * routing table
 */
static boolean_t
subnet_route_add(struct in_addr gateway, struct in_addr netaddr, 
		 struct in_addr netmask, const char * ifname);

static boolean_t
subnet_route_delete(struct in_addr gateway, struct in_addr netaddr, 
		    struct in_addr netmask);

/*
 * forward declarations
 */
static void
make_link_string(char * string_buffer, const uint8_t * hwaddr, int hwaddr_len);

static void
S_add_dhcp_parameters();

static void
configuration_changed(SCDynamicStoreRef session);

static boolean_t
get_media_status(const char * name, boolean_t * media_status);

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
config_method_media(Service_t * service_p, boolean_t network_changed);

static ipconfig_status_t
config_method_renew(Service_t * service_p);

static void
service_publish_clear(Service_t * service_p);

static int
inet_attach_interface(const char * ifname);

static int
inet_detach_interface(const char * ifname);

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
static IFState_t *
IFStateList_ifstate_with_name(IFStateList_t * list, const char * ifname,
			      int * where);
static void
IFState_service_free(IFState_t * ifstate, CFStringRef serviceID);

static Service_t *
IFState_service_with_ip(IFState_t * ifstate, struct in_addr iaddr);

static void
IFState_set_ssid(IFState_t * ifstate, CFStringRef ssid);

static void
S_linklocal_start(Service_t * parent_service_p, boolean_t no_allocate);

static CFStringRef
S_copy_ssid(CFStringRef ifname);

static int
S_remove_ip_address(const char * ifname, struct in_addr this_ip);

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
    int 	i;

    for (i = 0; i < ifl_count(S_interfaces); i++) {
	interface_t *	if_p = ifl_at_index(S_interfaces, i);
	int		link_length = if_link_length(if_p);
	
	if (hwlen != link_length) {
	    continue;
	}
	if (hwtype != if_link_arptype(if_p)) {
	    continue;
	}
	if (bcmp(hwaddr, if_link_address(if_p), link_length) == 0) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

/* 
 * Function: timestamp_fprintf
 *
 * Purpose:
 *   Print a timestamped message.
 */
void
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

void
my_log(int priority, const char *message, ...)
{
    va_list 		ap;

    if (priority == LOG_DEBUG) {
	if (G_IPConfiguration_verbose == FALSE)
	    return;
	priority = LOG_NOTICE;
    }
    else if (priority == LOG_INFO) {
	priority = LOG_NOTICE;
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
    int count;
    int i;

    count = CFArrayGetCount(arr);
    for (i = 0; i < count; i++) {
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

static __inline__ Boolean
my_CFEqual(CFTypeRef val1, CFTypeRef val2)
{
    if (val1 == NULL) {
	if (val2 == NULL) {
	    return (TRUE);
	}
	return (FALSE);
    }
    if (val2 == NULL) {
	return (FALSE);
    }
    if (CFGetTypeID(val1) != CFGetTypeID(val2)) {
	return (FALSE);
    }
    return (CFEqual(val1, val2));
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
			 0, FALSE, (UInt8 *)buf, sizeof(buf), &l);
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
			 0, FALSE, (UInt8 *)str, len, &l);
    str[l] = '\0';
    return (l);
}

ipconfig_status_t
validate_method_data_addresses(config_data_t * cfg, ipconfig_method_t method,
			       const char * ifname)
{
    if (cfg->data_len < sizeof(ipconfig_method_data_t)
	+ sizeof(struct in_addr) * 2) {
	my_log(LOG_DEBUG, "%s %s: method data too short (%d bytes)",
	       ipconfig_method_string(method), ifname, cfg->data_len);
	return (ipconfig_status_invalid_parameter_e);
    }
    if (cfg->data->n_ip == 0) {
	my_log(LOG_DEBUG, "%s %s: no IP addresses specified", 
	       ipconfig_method_string(method), ifname);
	return (ipconfig_status_invalid_parameter_e);
    }
    if (ip_valid(cfg->data->ip[0].addr) == FALSE) {
	my_log(LOG_DEBUG, "%s %s: invalid IP %s", 
	       ipconfig_method_string(method), ifname,
	       inet_ntoa(cfg->data->ip[0].addr));
	return (ipconfig_status_invalid_parameter_e);
    }
    return (ipconfig_status_success_e);
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
    if (_SC_CFStringIsValidDNSName(name) == FALSE) {
	my_CFRelease(&name);
	name = SCDynamicStoreCopyLocalHostName(session);
	if (name == NULL) {
	    goto done;
	}
	if (_SC_CFStringIsValidDNSName(name) == FALSE) {
	    goto done;
	}
	if (CFStringGetLength(name) > S_dhcp_local_hostname_length_max) {
	    /* don't exceed the maximum */
	    goto done;
	}
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
 ** ARP routine
 **/

static void
service_resolve_router_complete(void * arg1, void * arg2, 
				const arp_result_t * result)
{
    service_resolve_router_callback_t *	callback_func;
    interface_t *			if_p;
    Service_t *				service_p;
    router_arp_status_t			status;

    service_p = (Service_t *)arg1;
    callback_func = (service_resolve_router_callback_t *)arg2;
    if_p = service_interface(service_p);
    if (result->error) {
	my_log(LOG_ERR, "service_resolve_router_complete %s: ARP failed, %s", 
	       if_name(if_p),
	       arp_client_errmsg(result->client));
	status = router_arp_status_failed_e;
    }
    else if (result->in_use) {
	/* grab the latest router hardware address */
	bcopy(result->addr.target_hardware, service_p->router.hwaddr, 
	      if_link_length(if_p));
	service_router_set_all_valid(service_p);
	my_log(LOG_DEBUG, "service_resolve_router_complete %s: ARP "
	       IP_FORMAT ": response received", if_name(if_p),
	       IP_LIST(&service_p->router.iaddr));
	status = router_arp_status_success_e;
    }
    else {
	status = router_arp_status_no_response_e;
	my_log(LOG_DEBUG, "service_resolve_router_complete %s: ARP router " 
	       IP_FORMAT ": no response", if_name(if_p),
	       IP_LIST(&service_p->router.iaddr));
    }
    (*callback_func)(service_p, status);
    return;
}

boolean_t
service_resolve_router(Service_t * service_p, arp_client_t * arp,
		       service_resolve_router_callback_t * callback_func,
		       struct in_addr our_ip)
{
    interface_t *	if_p = service_interface(service_p);
    struct in_addr	router_ip;

    if (G_discover_and_publish_router_mac_address == FALSE) {
	/* don't bother */
	return (FALSE);
    }

    service_router_clear_arp_verified(service_p);
    if (service_router_is_iaddr_valid(service_p) == 0) {
	my_log(LOG_NOTICE,
	       "service_resolve_router %s: IP address missing", if_name(if_p));
	return (FALSE);
    }
    router_ip = service_router_iaddr(service_p);
    my_log(LOG_DEBUG, "service_resolve_router %s: sender " IP_FORMAT 
	   " target " IP_FORMAT " started", 
	   if_name(if_p), IP_LIST(&our_ip), IP_LIST(&router_ip));
    arp_client_resolve(arp, service_resolve_router_complete,
		       service_p, callback_func, our_ip, router_ip,
		       S_discover_router_mac_address_secs);
    return (TRUE);
}

struct in_addr *
get_router_from_options(dhcpol_t * options_p, struct in_addr our_ip)
{
    struct in_addr *	router_p;
    
    router_p = dhcpol_find_with_length(options_p, dhcptag_router_e, 
				       sizeof(*router_p));
    if (router_p == NULL) {
	goto failed;
    }
    if (router_p->s_addr == our_ip.s_addr) {
	/* proxy arp, use DNS server instead */
	router_p = dhcpol_find_with_length(options_p,
					   dhcptag_domain_name_server_e, 
					   sizeof(*router_p));
	if (router_p == NULL) {
	    goto failed;
	}
    }
    if (router_p->s_addr == 0 || router_p->s_addr == 0xffffffff) {
	goto failed;
    }
    return (router_p);

 failed:
    return (NULL);
}

boolean_t
service_update_router_address(Service_t * service_p,
			      struct saved_pkt * saved_p)
{
    struct in_addr *		router_p;

    router_p = get_router_from_options(&saved_p->options, saved_p->our_ip);
    if (router_p == NULL) {
	goto failed;
    }
    if (service_router_all_valid(service_p)
	&& router_p->s_addr == service_router_iaddr(service_p).s_addr) {
	/* router is the same, no need to update */
	return (FALSE);
    }
    service_router_clear(service_p);
    service_router_set_iaddr(service_p, *router_p);
    service_router_set_iaddr_valid(service_p);
    return (TRUE);

 failed:
    service_router_clear(service_p);
    return (FALSE);
}

/**
 ** DHCP Lease routines
 **/
#define DHCPCLIENT_LEASES_DIR		"/var/db/dhcpclient/leases"
#define DHCPCLIENT_LEASE_FILE_FMT	DHCPCLIENT_LEASES_DIR "/%s-%s"

static void
dhcp_lease_init()
{
    if (create_path(DHCPCLIENT_LEASES_DIR, 0700) < 0) {
	my_log(LOG_DEBUG, "failed to create " 
	       DHCPCLIENT_LEASES_DIR ", %s (%d)", strerror(errno), errno);
	return;
    }
}

/* required properties: */
#define kLeaseStartDate			CFSTR("LeaseStartDate")
#define kPacketData			CFSTR("PacketData")
#define kRouterHardwareAddress		CFSTR("RouterHardwareAddress")

/* informative properties: */
#define kLeaseLength			CFSTR("LeaseLength")
#define kIPAddress			CFSTR("IPAddress")
#define kRouterIPAddress		CFSTR("RouterIPAddress")

/*
 * Function: DHCPLeaseCreateWithDictionary
 * Purpose:
 *   Instantiate a new DHCPLease structure corresponding to the given
 *   dictionary.  Validates that required properties are present,
 *   returns NULL if those checks fail.
 */
static DHCPLeaseRef
DHCPLeaseCreateWithDictionary(CFDictionaryRef dict)
{
    CFDataRef			hwaddr_data;
    dhcp_lease_time_t		lease_time;
    DHCPLeaseRef		lease_p = NULL;
    CFDataRef			pkt_data;
    CFRange			pkt_data_range;
    struct in_addr *		router_p;
    CFDateRef			start_date;
    dhcp_lease_time_t		t1_time;
    dhcp_lease_time_t		t2_time;

    /* get the lease start time */
    start_date = CFDictionaryGetValue(dict, kLeaseStartDate);
    if (isA_CFDate(start_date) == NULL) {
	goto failed;
    }
    /* get the packet data */
    pkt_data = CFDictionaryGetValue(dict, kPacketData);
    if (isA_CFData(pkt_data) == NULL) {
	goto failed;
    }
    pkt_data_range.location = 0;
    pkt_data_range.length = CFDataGetLength(pkt_data);
    if (pkt_data_range.length < sizeof(struct dhcp)) {
	goto failed;
    }
    lease_p = (DHCPLeaseRef)malloc(sizeof(*lease_p) - 1
				   + pkt_data_range.length);
    bzero(lease_p, sizeof(*lease_p) - 1);

    /* copy the packet data */
    CFDataGetBytes(pkt_data, pkt_data_range, lease_p->pkt);
    lease_p->pkt_length = pkt_data_range.length;

    /* get the lease information and router IP address */
    lease_p->lease_start = (absolute_time_t)CFDateGetAbsoluteTime(start_date);
    { /* parse/retrieve options */
	dhcpol_t			options;
	
	(void)dhcpol_parse_packet(&options, (void *)lease_p->pkt,
				  pkt_data_range.length, NULL);
	dhcp_get_lease_from_options(&options, &lease_time, &t1_time, &t2_time);
	router_p = get_router_from_options(&options, lease_p->our_ip);
	dhcpol_free(&options);
    }
    lease_p->lease_length = lease_time;

    /* get the IP address */
    lease_p->our_ip = ((struct dhcp *)lease_p->pkt)->dp_yiaddr;

    /* get the router information */
    if (router_p != NULL) {
	CFRange		hwaddr_range;

	lease_p->router_ip = *router_p;
	/* get the router hardware address */
	hwaddr_data = CFDictionaryGetValue(dict, kRouterHardwareAddress);
	hwaddr_range.length = 0;
	if (isA_CFData(hwaddr_data) != NULL) {
	    hwaddr_range.length = CFDataGetLength(hwaddr_data);
	}
	if (hwaddr_range.length > 0) {
	    hwaddr_range.location = 0;
	    if (hwaddr_range.length > sizeof(lease_p->router_hwaddr)) {
		hwaddr_range.length = sizeof(lease_p->router_hwaddr);
	    }
	    lease_p->router_hwaddr_length = hwaddr_range.length;
	    CFDataGetBytes(hwaddr_data, hwaddr_range, lease_p->router_hwaddr);
	}
    }
    return (lease_p);

 failed:
    if (lease_p != NULL) {
	free(lease_p);
    }
    return (NULL);
}

void
DHCPLeaseSetNAK(DHCPLeaseRef lease_p, int nak)
{
    lease_p->nak = nak;
    return;
}

/*
 * Function: DHCPLeaseCreate
 * Purpose:
 *   Instantiate a new DHCPLease structure corresponding to the given
 *   information.
 */
static DHCPLeaseRef
DHCPLeaseCreate(struct in_addr our_ip, struct in_addr router_ip,
		const uint8_t * router_hwaddr, int router_hwaddr_length,
		absolute_time_t lease_start, 
		dhcp_lease_time_t lease_length,
		const uint8_t * pkt, int pkt_size)
{
    DHCPLeaseRef		lease_p = NULL;
    int				lease_data_length;

    lease_data_length = offsetof(DHCPLease, pkt) + pkt_size;
    lease_p = (DHCPLeaseRef)malloc(lease_data_length);
    bzero(lease_p, lease_data_length);
    lease_p->our_ip = our_ip;
    lease_p->router_ip = router_ip;
    lease_p->lease_start = lease_start;
    lease_p->lease_length = lease_length;
    bcopy(pkt, lease_p->pkt, pkt_size);
    lease_p->pkt_length = pkt_size;
    if (router_hwaddr != NULL && router_hwaddr_length != 0) {
	if (router_hwaddr_length > sizeof(lease_p->router_hwaddr)) {
	    router_hwaddr_length = sizeof(lease_p->router_hwaddr);
	}
	lease_p->router_hwaddr_length = router_hwaddr_length;
	bcopy(router_hwaddr, lease_p->router_hwaddr, router_hwaddr_length);
    }
    return (lease_p);
}

static CFDictionaryRef
DHCPLeaseCopyDictionary(DHCPLeaseRef lease_p)
{
    CFDataRef			data;
    CFDateRef			date;
    CFMutableDictionaryRef	dict;
    CFNumberRef			num;
    CFStringRef			str;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    /* set the IP address */
    str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"),
				   inet_ntoa(lease_p->our_ip));
    CFDictionarySetValue(dict, kIPAddress, str);
    CFRelease(str);


    /* set the lease start date */
    date = CFDateCreate(NULL, lease_p->lease_start);
    CFDictionarySetValue(dict, kLeaseStartDate, date);
    CFRelease(date);

    /* set the lease length */
    num = CFNumberCreate(NULL, kCFNumberSInt32Type, &lease_p->lease_length);
    CFDictionarySetValue(dict, kLeaseLength, num);
    CFRelease(num);

    /* set the packet data */
    data = CFDataCreateWithBytesNoCopy(NULL, lease_p->pkt, lease_p->pkt_length,
				       kCFAllocatorNull);
    CFDictionarySetValue(dict, kPacketData, data);
    CFRelease(data);

    if (lease_p->router_ip.s_addr != 0) {
	/* set the router IP address */
	str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"),
				       inet_ntoa(lease_p->router_ip));
	CFDictionarySetValue(dict, kRouterIPAddress, str);
	CFRelease(str);

	if (lease_p->router_hwaddr_length > 0) {
	    /* set the router hardware address */
	    data = CFDataCreateWithBytesNoCopy(NULL, lease_p->router_hwaddr,
					       lease_p->router_hwaddr_length,
					       kCFAllocatorNull);
	    CFDictionarySetValue(dict, kRouterHardwareAddress, data);
	    CFRelease(data);
	}
    }
    return (dict);
}

static void
DHCPLeasePrint(DHCPLeaseRef lease_p)
{
    printf("IP %s Start %d Length", inet_ntoa(lease_p->our_ip),
	   (int)lease_p->lease_start);
    if (lease_p->lease_length == DHCP_INFINITE_LEASE) {
	printf(" infinite");
    }
    else {
	printf(" %d", (int)lease_p->lease_length);
    }

    if (lease_p->router_ip.s_addr != 0) {
	printf(" Router IP %s", inet_ntoa(lease_p->router_ip));
	if (lease_p->router_hwaddr_length > 0) {
	    char	link_string[MAX_LINK_ADDR_LEN * 3];

	    make_link_string(link_string, lease_p->router_hwaddr,
			     lease_p->router_hwaddr_length);
	    printf(" MAC %s", link_string);
	}
    }
    printf("\n");
}

static void
DHCPLeaseListPrint(DHCPLeaseListRef list_p)
{
    int		count;
    int		i;
    
    count = dynarray_count(list_p);
    printf("There are %d leases\n", count);
    for (i = 0; i < count; i++) {
	DHCPLeaseRef	lease_p = dynarray_element(list_p, i);
	printf("%d. ", i + 1);
	DHCPLeasePrint(lease_p);
    }
    return;
}

static boolean_t
DHCPLeaseListGetPath(const char * ifname,
		     uint8_t cid_type, const void * cid, int cid_length,
		     char * filename, int filename_size)
{
    char *			idstr;
    char			idstr_scratch[128];
    
    idstr = identifierToStringWithBuffer(cid_type, cid, cid_length,
					 idstr_scratch, sizeof(idstr_scratch));
    if (idstr == NULL) {
	return (FALSE);
    }
    snprintf(filename, filename_size, DHCPCLIENT_LEASE_FILE_FMT, ifname,
	     idstr);
    if (idstr != idstr_scratch) {
	free(idstr);
    }
    return (TRUE);
}

void
DHCPLeaseListInit(DHCPLeaseListRef list_p)
{
    dynarray_init(list_p, free, NULL);
    return;
}

void
DHCPLeaseListFree(DHCPLeaseListRef list_p)
{
    dynarray_free(list_p);
}

/*
 * Function: DHCPLeaseListRemoveStaleLeases
 * Purpose:
 *   Scans the list of leases removing any that are no longer valid.
 */
static void
DHCPLeaseListRemoveStaleLeases(DHCPLeaseListRef list_p)
{
    int				count;
    absolute_time_t 		current_time;
    int				i;

    count = dynarray_count(list_p);
    if (count == 0) {
	return;
    }
    current_time = timer_current_secs();
    i = 0;
    while (i < count) {
	DHCPLeaseRef	lease_p = dynarray_element(list_p, i);

	/* check the lease expiration */
	if (lease_p->lease_length != DHCP_INFINITE_LEASE
	    && current_time >= (lease_p->lease_start + lease_p->lease_length)) {
	    /* lease is expired */
	    if (G_IPConfiguration_verbose) {
		my_log(LOG_NOTICE, "Removing Stale Lease "
		       IP_FORMAT " Router " IP_FORMAT,
		       IP_LIST(&lease_p->our_ip),
		       IP_LIST(&lease_p->router_ip));
	    }
	    dynarray_free_element(list_p, i);
	    count--;
	}
	else {
	    i++;
	}
    }
    return;
}

/* 
 * Function: DHCPLeaseListRead
 *
 * Purpose:
 *   Read the single DHCP lease for the given interface/client_id into a
 *   DHCPLeaseList structure.  This lease is marked as "tentative" because
 *   we have no idea whether the lease is still good or not, since another
 *   version of the OS (or another OS) could have had additional communication
 *   with the DHCP server, invalidating our notion of the lease.  It also
 *   affords a simple, self-cleansing mechanism to clear out the set of
 *   leases we keep track.
 */
void
DHCPLeaseListRead(DHCPLeaseListRef list_p,
		  const char * ifname,
		  uint8_t cid_type, const void * cid, int cid_length)
{
    char			filename[PATH_MAX];
    CFDictionaryRef		lease_dict = NULL;
    DHCPLeaseRef		lease_p;
    struct in_addr		lease_ip;

    DHCPLeaseListInit(list_p);
    if (DHCPLeaseListGetPath(ifname, cid_type, cid, cid_length,
			     filename, sizeof(filename)) == FALSE) {
	goto done;
    }
    lease_dict = my_CFPropertyListCreateFromFile(filename);
    if (isA_CFDictionary(lease_dict) == NULL) {
	goto done;
    }
    lease_p = DHCPLeaseCreateWithDictionary(lease_dict);
    if (lease_p == NULL) {
	goto done;
    }
    lease_p->tentative = TRUE;
    dynarray_add(list_p, lease_p);
    if (G_debug) {
	DHCPLeaseListPrint(list_p);
    }
    lease_ip = lease_p->our_ip;
    DHCPLeaseListRemoveStaleLeases(list_p);
    if (DHCPLeaseListCount(list_p) == 0) {
	IFState_t * 	ifstate;

	/* if no service on this interface refers to this IP, remove the IP */
	ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, ifname, NULL);
	if (ifstate != NULL
	    && IFState_service_with_ip(ifstate, lease_ip) == NULL) {
	    S_remove_ip_address(if_name(ifstate->if_p), lease_ip);
	}
    }

 done:
    my_CFRelease(&lease_dict);
    return;
}

/* 
 * Function: DHCPLeaseListWrite
 *
 * Purpose:
 *   Write the last DHCP lease in the list for the given interface/client_id.
 *   We only save the last (current) lease.  See the comments for 
 *   DHCPLeaseListRead above for more information.
 */
void
DHCPLeaseListWrite(DHCPLeaseListRef list_p,
		   const char * ifname,
		   uint8_t cid_type, const void * cid, int cid_length)
{
    int			count;
    char		filename[PATH_MAX];
    CFDictionaryRef	lease_dict;
    DHCPLeaseRef	lease_p;
    
    if (DHCPLeaseListGetPath(ifname, cid_type, cid, cid_length,
			     filename, sizeof(filename)) == FALSE) {
	return;
    }
    DHCPLeaseListRemoveStaleLeases(list_p);
    count = dynarray_count(list_p);
    if (count == 0) {
	unlink(filename);
	return;
    }
    lease_p = dynarray_element(list_p, count - 1);
    lease_dict = DHCPLeaseCopyDictionary(lease_p);
    if (my_CFPropertyListWriteFile(lease_dict, filename) < 0) {
	/*
	 * An ENOENT error is expected on a read-only filesystem.  All 
	 * other errors should be reported.
	 */
	if (errno != ENOENT) {
	    my_log(LOG_NOTICE, "my_CFPropertyListWriteFile(%s) failed, %s", 
		   filename, strerror(errno));
	}
    }
    my_CFRelease(&lease_dict);
    return;
}

/*
 * Function: DHCPLeaseListCopyARPAddressInfo
 * Purpose:
 *   Returns a list of arp_address_info_t's corresponding to each
 *   discoverable lease.
 */
arp_address_info_t *
DHCPLeaseListCopyARPAddressInfo(DHCPLeaseListRef list_p, bool tentative_ok,
				int * ret_count)
{
    int				arp_info_count;
    arp_address_info_t *	arp_info_p;
    int				count;
    int				i;
    arp_address_info_t *	info_p;

    DHCPLeaseListRemoveStaleLeases(list_p);
    count = dynarray_count(list_p);
    if (count == 0) {
	*ret_count = 0;
	return (NULL);
    }
    arp_info_p = (arp_address_info_t *)malloc(sizeof(*arp_info_p) * count);
    arp_info_count = 0;
    info_p = arp_info_p;
    for (i = 0; i < count; i++) {
	DHCPLeaseRef	lease_p = dynarray_element(list_p, i);

	if (lease_p->router_ip.s_addr == 0
	    || lease_p->router_hwaddr_length == 0) {
	    /* can't use this with ARP discovery */
	    if (G_IPConfiguration_verbose) {
		my_log(LOG_NOTICE, "ignoring lease for " IP_FORMAT,
		       IP_LIST(&lease_p->our_ip));
	    }
	    continue;
	}
	if (lease_p->tentative && tentative_ok == FALSE) {
	    /* ignore tentative lease */
	    continue;
	}
	info_p->sender_ip = lease_p->our_ip;
	info_p->target_ip = lease_p->router_ip;
	bcopy(lease_p->router_hwaddr, info_p->target_hardware,
	      lease_p->router_hwaddr_length);
	arp_info_count++;
	info_p++;
    }
    if (arp_info_count == 0) {
	free(arp_info_p);
	arp_info_p = NULL;
    }
    *ret_count = arp_info_count;
    return (arp_info_p);
}

/*
 * Function: DHCPLeaseListFindLease
 * Purpose:
 *   Find a lease corresponding to the supplied information.
 */
int
DHCPLeaseListFindLease(DHCPLeaseListRef list_p, struct in_addr our_ip,
		       struct in_addr router_ip,
		       const uint8_t * router_hwaddr, int router_hwaddr_length)
{
    int			count;
    int			i;
    boolean_t		private_ip = ip_is_private(our_ip);

    count = dynarray_count(list_p);
    for (i = 0; i < count; i++) {
	DHCPLeaseRef	lease_p = dynarray_element(list_p, i);

	if (lease_p->our_ip.s_addr != our_ip.s_addr) {
	    /* IP doesn't match */
	    continue;
	}
	if (private_ip == FALSE) {
	    /* lease for public IP is unique */
	    return (i);
	}
	if (lease_p->router_ip.s_addr != router_ip.s_addr) {
	    /* router IP doesn't match (or one is set the other isn't)*/
	    continue;
	}
	if (router_ip.s_addr == 0) {
	    /* found lease with no router information */
	    return (i);
	}
	if (lease_p->router_hwaddr_length != router_hwaddr_length) {
	    /* one has router hwaddr, other doesn't */
	    continue;
	}
	if (router_hwaddr == NULL || router_hwaddr_length == 0) {
	    /* found lease with router IP but no router hwaddr */
	    return (i);
	}
	if (bcmp(lease_p->router_hwaddr, router_hwaddr, router_hwaddr_length)
	    == 0) {
	    /* exact match on IP, router IP, router hwaddr */
	    return (i);
	}
    }
    return (-1);
}

/*
 * Function: DHCPLeaseShouldBeRemoved
 * Purpose:
 *   Given an existing lease entry 'existing_p' and the one to be added 'new_p',
 *   determine whether the existing lease entry should be removed.
 *   
 *   The criteria for removing the lease entry:
 *   1) No router information is specified.  This entry is useless for lease
 *      detection.   If such a lease exists, it only makes sense to have
 *      one of them, the most recently used lease.  It will be added to the
 *      end of the list in that case.
 *   2) Lease was NAK'd.  The DHCP server NAK'd this lease and it's for the
 *      same network i.e. same router MAC.
 *   3) Lease is for a public IP and is the same as the new lease.
 *   4) Lease has the same router IP/MAC address as an existing lease.  We
 *      can only sensibly have a single lease for a particular network, so
 *      eliminate redundant ones.
 */
static boolean_t
DHCPLeaseShouldBeRemoved(DHCPLeaseRef existing_p, DHCPLeaseRef new_p, 
			 boolean_t private_ip)
{
    if (existing_p->router_ip.s_addr == 0
	|| existing_p->router_hwaddr_length == 0) {
	if (G_IPConfiguration_verbose) {
	    my_log(LOG_NOTICE,
		   "Removing lease with no router for IP address "
		   IP_FORMAT, IP_LIST(&existing_p->our_ip));
	}
	return (TRUE);
    }
    if (existing_p->nak) {
	boolean_t		ignore = FALSE;

	existing_p->nak = FALSE;
	if (ip_is_private(existing_p->our_ip) != private_ip) {
	    /* one IP is private, the other is public, ignore NAK */
	    ignore = TRUE;
	}
	else if (new_p->router_hwaddr_length != 0
		 && bcmp(existing_p->router_hwaddr, new_p->router_hwaddr,
			 existing_p->router_hwaddr_length) != 0) {
	    /* router MAC on NAK'd lease is different, so ignore NAK */
	    ignore = TRUE;
	}
	if (ignore) {
	    if (G_IPConfiguration_verbose) {
		my_log(LOG_NOTICE, "Ignoring NAK on IP address "
		       IP_FORMAT, IP_LIST(&existing_p->our_ip));
	    }
	}
	else {
	    if (G_IPConfiguration_verbose) {
		my_log(LOG_NOTICE, "Removing NAK'd lease for IP address "
		       IP_FORMAT, IP_LIST(&existing_p->our_ip));
	    }
	    return (TRUE);
	}
    }
    if (private_ip == FALSE
	&& new_p->our_ip.s_addr == existing_p->our_ip.s_addr) {
	/* public IP's are the same, remove it */
	if (G_IPConfiguration_verbose) {
	    my_log(LOG_NOTICE, "Removing lease for public IP address "
		   IP_FORMAT, IP_LIST(&existing_p->our_ip));
	}
	return (TRUE);
    }
    if (new_p->router_ip.s_addr == 0
	|| new_p->router_hwaddr_length == 0) {
	/* new lease doesn't have a router */
	return (FALSE);
    }
    if (bcmp(new_p->router_hwaddr, existing_p->router_hwaddr, 
	     new_p->router_hwaddr_length) == 0) {
	if (G_IPConfiguration_verbose) {
	    if (new_p->our_ip.s_addr == existing_p->our_ip.s_addr) {
		my_log(LOG_NOTICE,
		       "Removing lease with same router for IP address "
		       IP_FORMAT, IP_LIST(&existing_p->our_ip));
	    }
	    else {
		my_log(LOG_NOTICE,
		       "Removing lease with same router, old IP "
		       IP_FORMAT " new IP " IP_FORMAT,
		       IP_LIST(&existing_p->our_ip),
		       IP_LIST(&new_p->our_ip));
	    }
	}
	return (TRUE);
    }
    return (FALSE);
}

/* 
 * Function: DHCPLeaseListUpdateLease
 *
 * Purpose:
 *   Update the lease entry for the given lease in the in-memory lease database.
 */
void
DHCPLeaseListUpdateLease(DHCPLeaseListRef list_p, struct in_addr our_ip,
			 struct in_addr router_ip,
			 const uint8_t * router_hwaddr,
			 int router_hwaddr_length,
			 absolute_time_t lease_start,
			 dhcp_lease_time_t lease_length,
			 const uint8_t * pkt, int pkt_size)
{
    int			count;
    int			i;
    DHCPLeaseRef	lease_p;
    boolean_t		private_ip = ip_is_private(our_ip);

    lease_p = DHCPLeaseCreate(our_ip, router_ip,
			      router_hwaddr, router_hwaddr_length,
			      lease_start, lease_length, pkt, pkt_size);
    /* scan lease list to eliminate NAK'd, incomplete, and duplicate leases */
    count = dynarray_count(list_p);
    for (i = 0; i < count; i++) {
	DHCPLeaseRef	scan_p = dynarray_element(list_p, i);

	if (DHCPLeaseShouldBeRemoved(scan_p, lease_p, private_ip)) {
	    dynarray_free_element(list_p, i);
	    i--;
	    count--;
	}
    }
    if (G_IPConfiguration_verbose) {
	my_log(LOG_NOTICE, "Saving lease for " IP_FORMAT,
	       IP_LIST(&lease_p->our_ip));
    }
    dynarray_add(list_p, lease_p);
    return;
}

/* 
 * Function: DHCPLeaseListRemoveLease
 *
 * Purpose:
 *   Remove the lease entry for the given lease.
 */
void
DHCPLeaseListRemoveLease(DHCPLeaseListRef list_p,
			 struct in_addr our_ip,
			 struct in_addr router_ip,
			 const uint8_t * router_hwaddr,
			 int router_hwaddr_length)
{
    int		where;

    /* remove the old information if it's there */
    where = DHCPLeaseListFindLease(list_p, our_ip, router_ip,
				   router_hwaddr, router_hwaddr_length);
    if (where != -1) {
	if (G_IPConfiguration_verbose) {
	    DHCPLeaseRef lease_p = DHCPLeaseListElement(list_p, where);

	    my_log(LOG_NOTICE, "Removing lease for " IP_FORMAT,
		   IP_LIST(&lease_p->our_ip));
	}
	dynarray_free_element(list_p, where);
    }
    return;
}

/* 
 * Function: DHCPLeaseListRemoveAllButLastLease
 * Purpose:
 *   Remove all leases except the last one (the most recently used one),
 *   and mark it tentative.
 */
void
DHCPLeaseListRemoveAllButLastLease(DHCPLeaseListRef list_p)
{
    int			count;
    int			i;
    DHCPLeaseRef	lease_p;

    count = DHCPLeaseListCount(list_p);
    if (count == 0) {
	return;
    }
    for (i = 0; i < (count - 1); i++) {
	if (G_IPConfiguration_verbose) {
	    lease_p = DHCPLeaseListElement(list_p, 0);	    
	    my_log(LOG_NOTICE, "Removing lease #%d for IP address "
		   IP_FORMAT, i + 1, IP_LIST(&lease_p->our_ip));
	}
	dynarray_free_element(list_p, 0);
    }
    lease_p = DHCPLeaseListElement(list_p, 0);
    lease_p->tentative = TRUE;
    return;
}

/**
 ** Utility routines
 **/

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
ifflags_set(int s, const char * name, short flags)
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
siocprotoattach(int s, const char * name)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCPROTOATTACH, &ifr));
}

static int
siocprotodetach(int s, const char * name)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCPROTODETACH, &ifr));
}

static int
siocautoaddr(int s, const char * name, int value)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    ifr.ifr_data = (caddr_t)(intptr_t)value;
    return (ioctl(s, SIOCAUTOADDR, &ifr));
}

static int
inet_difaddr(int s, const char * name, const struct in_addr * addr)
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

static int
inet_aifaddr(int s, const char * name, const struct in_addr * addr, 
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

/**
 ** Service_*, IFState_* routines
 **/
static void
Service_free(void * arg)
{
    IFState_t *		ifstate;
    Service_t *		service_p = (Service_t *)arg;
    
    ifstate = service_ifstate(service_p);
    SCLog(G_IPConfiguration_verbose, LOG_NOTICE, CFSTR("Service_free(%@)"), 
	  service_p->serviceID);
    if (ifstate != NULL && ifstate->linklocal_service_p == service_p) {
	ifstate->linklocal_service_p = NULL;
    }
    service_p->free_in_progress = TRUE;
    config_method_stop(service_p);
    service_publish_clear(service_p);
#if ! TARGET_OS_EMBEDDED
    if (service_p->user_rls) {
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(), service_p->user_rls,
			      kCFRunLoopDefaultMode);
	my_CFRelease(&service_p->user_rls);
    }
    if (service_p->user_notification != NULL) {
	CFUserNotificationCancel(service_p->user_notification);
	my_CFRelease(&service_p->user_notification);
    }
#endif /* ! TARGET_OS_EMBEDDED */
    my_CFRelease(&service_p->serviceID);
    my_CFRelease(&service_p->parent_serviceID);
    my_CFRelease(&service_p->child_serviceID);
    free(service_p);
    return;
}

static Service_t *
Service_init(IFState_t * ifstate, CFStringRef serviceID,
	     ipconfig_method_t method,
	     ipconfig_method_data_t * method_data,
	     unsigned int method_data_len,
	     Service_t * parent_service_p, ipconfig_status_t * status_p)
{
    Service_t *		service_p = NULL;
    ipconfig_status_t	status = ipconfig_status_success_e;

    switch (method) {
    case ipconfig_method_bootp_e:
	if (if_link_type(ifstate->if_p) == IFT_IEEE1394) {
	    /* can't do BOOTP over firewire */
	    status = ipconfig_status_operation_not_supported_e;
	    goto failed;
	}
	break;
    case ipconfig_method_linklocal_e:
	if (ifstate->linklocal_service_p != NULL) {
	    IFState_service_free(ifstate,
				 ifstate->linklocal_service_p->serviceID);
	    /* side-effect: ifstate->linklocal_service_p = NULL */
	}
	break;
    default:
	break;
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
	my_CFRelease(&parent_service_p->child_serviceID);
	parent_service_p->child_serviceID 
	    = (void *)CFRetain(service_p->serviceID);
	if (service_p->method == ipconfig_method_linklocal_e) {
	    ifstate->linklocal_service_p = service_p;
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

/*
 * Function: IFState_service_matching_method
 * Purpose:
 *   Find a service that "matches" the given requested method.  A service
 *   "matches" if the method types are not manual (i.e. BOOTP, DHCP),
 *   or the method types are manual (Manual, Inform, Failover), and the
 *   requested IP address matches.
 * 
 *   If the parameter "just_dynamic" is TRUE, only match any dynamic
 *   service, otherwise match all services.
 *
 */
static Service_t *
IFState_service_matching_method(IFState_t * ifstate,
				ipconfig_method_t method,
				ipconfig_method_data_t * method_data,
				unsigned int method_data_len,
				boolean_t just_dynamic)
{
    boolean_t		is_manual;
    int			j;

    is_manual = ipconfig_method_is_manual(method);
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	boolean_t	this_is_manual;
	Service_t *	service_p = dynarray_element(&ifstate->services, j);

	if (just_dynamic && service_p->is_dynamic == FALSE) {
	    continue;
	}
	this_is_manual = ipconfig_method_is_manual(service_p->method);
	if (is_manual) {
	    if (this_is_manual
		&& (method_data->ip[0].addr.s_addr
		    == service_requested_ip_addr(service_p).s_addr)) {
		return (service_p);
	    }
	}
	else if (ipconfig_method_is_manual(service_p->method) == FALSE) {
	    return (service_p);
	}

    }
    return (NULL);
}

/*
 * Function: IFState_service_with_method
 * Purpose:
 *   Find a service with the given method and method args.
 * 
 *   If the parameter "just_dynamic" is TRUE, only match any dynamic
 *   service, otherwise match all services.
 *
 */
static Service_t *
IFState_service_with_method(IFState_t * ifstate,
			    ipconfig_method_t method,
			    ipconfig_method_data_t * method_data,
			    unsigned int method_data_len,
			    boolean_t just_dynamic)
{
    int			j;
    boolean_t		is_manual;

    is_manual = ipconfig_method_is_manual(method);
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	Service_t *	service_p = dynarray_element(&ifstate->services, j);

	if (just_dynamic && service_p->is_dynamic == FALSE) {
	    continue;
	}
	if (method == service_p->method) {
	    if (is_manual == FALSE
		|| (method_data->ip[0].addr.s_addr
		    == service_requested_ip_addr(service_p).s_addr)) {
		return (service_p);
	    }
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

static ipconfig_status_t
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
	    ifstate->startup_ready = TRUE;
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
    const char * 	ifname = if_name(ifstate->if_p);
    link_status_t	link = {FALSE, FALSE};

    link.valid = get_media_status(ifname, &link.active);
    if (link.valid == FALSE) {
	my_log(LOG_DEBUG, "%s link is unknown", ifname);
    }
    else {
	my_log(LOG_DEBUG, "%s link is %s", ifname, link.active ? "up" : "down");
    }
    ifstate->link = link;
    if (if_is_wireless(ifstate->if_p)) {
	CFStringRef	ssid;

	ssid = S_copy_ssid(ifstate->ifname);
	if (G_IPConfiguration_verbose) {
	    if (ssid != NULL) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("%s: SSID is %@"), 
		      if_name(ifstate->if_p), ssid);
	    }
	    else {
		my_log(LOG_NOTICE, "%s: no SSID",
		       if_name(ifstate->if_p));
	    }
	}
	/* remember the ssid */
	IFState_set_ssid(ifstate, ssid);
	my_CFRelease(&ssid);
    }
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
    ifstate->ifname = CFStringCreateWithCString(NULL, if_name(if_p),
						kCFStringEncodingMacRoman);
    IFState_update_media_status(ifstate);
    dynarray_init(&ifstate->services, Service_free, NULL);
    return (ifstate);
}

static void
IFState_set_ssid(IFState_t * ifstate, CFStringRef ssid)
{
    if (ssid != NULL) {
	CFRetain(ssid);
    }
    if (ifstate->ssid != NULL) {
	CFRelease(ifstate->ssid);
    }
    ifstate->ssid = ssid;
    return;
}

static void
IFState_free(void * arg)
{
    IFState_t *		ifstate = (IFState_t *)arg;
    
    SCLog(G_IPConfiguration_verbose, LOG_NOTICE, CFSTR("IFState_free(%s)"), 
	  if_name(ifstate->if_p));
    IFState_services_free(ifstate);
    my_CFRelease(&ifstate->ifname);
    IFState_set_ssid(ifstate, NULL);
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
IFStateList_ifstate_with_name(IFStateList_t * list, const char * ifname,
			      int * where)
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
IFStateList_ifstate_free(IFStateList_t * list, const char * ifname)
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
	iaddr_p = (struct in_addr *)
	    dhcpol_find_with_length(&options, 
				    dhcptag_server_identifier_e, 
				    sizeof(*iaddr_p));
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
count_params(dhcpol_t * options, const uint8_t * tags, int size)
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
	if (G_IPConfiguration_verbose) {
	    SCLog(TRUE, LOG_NOTICE,
		  CFSTR("service_publish_clear: Remove = %@"), array);
	}
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

static void
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
#if ! TARGET_OS_EMBEDDED
	     CFStringRef smb_key, CFDictionaryRef smb_dict, 
#endif /* ! TARGET_OS_EMBEDDED */
	     CFStringRef dhcp_key, CFDictionaryRef dhcp_dict)
{
    CFMutableDictionaryRef	keys_to_set = NULL;
    CFMutableArrayRef		keys_to_remove = NULL;
    
    if (G_IPConfiguration_verbose) {
	if (ipv4_dict != NULL) {
	    SCLog(TRUE, LOG_NOTICE, CFSTR("%@ = %@"),  ipv4_key, ipv4_dict);
	}
	if (dns_dict != NULL) {
	    SCLog(TRUE, LOG_NOTICE, CFSTR("%@ = %@"),  dns_key, dns_dict);
	}
#if ! TARGET_OS_EMBEDDED
	if (smb_dict != NULL) {
	    SCLog(TRUE, LOG_NOTICE, CFSTR("%@ = %@"),  smb_key, smb_dict);
	}
#endif /* ! TARGET_OS_EMBEDDED */
	if (dhcp_dict != NULL) {
	    SCLog(TRUE, LOG_NOTICE, CFSTR("%@ = %@"), dhcp_key, dhcp_dict);
	}
    }
    keys_to_set = CFDictionaryCreateMutable(NULL, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
    keys_to_remove = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (keys_to_set == NULL || keys_to_remove == NULL) {
	goto done;
    }
    update_key(S_scd_session, ipv4_key, ipv4_dict, keys_to_set, keys_to_remove);
    update_key(S_scd_session, dns_key, dns_dict, keys_to_set, keys_to_remove);
#if ! TARGET_OS_EMBEDDED
    update_key(S_scd_session, smb_key, smb_dict, keys_to_set, keys_to_remove);
#endif /* ! TARGET_OS_EMBEDDED */
    update_key(S_scd_session, dhcp_key, dhcp_dict, keys_to_set, keys_to_remove);
    if (CFArrayGetCount(keys_to_remove) > 0 
	|| CFDictionaryGetCount(keys_to_set) > 0) {
	SCDynamicStoreSetMultiple(S_scd_session,
				  keys_to_set,
				  keys_to_remove,
				  NULL);
	if (G_IPConfiguration_verbose) {
	    SCLog(TRUE, LOG_NOTICE, CFSTR("publish_keys: Set = %@"),
		  keys_to_set);
	    SCLog(TRUE, LOG_NOTICE, CFSTR("publish_keys: Remove = %@"),
		  keys_to_remove);
	}
    }
 done:
    my_CFRelease(&keys_to_remove);
    my_CFRelease(&keys_to_set);
    return;
}

static void
publish_service(CFStringRef serviceID, CFDictionaryRef ipv4_dict,
		CFDictionaryRef dns_dict,
#if ! TARGET_OS_EMBEDDED
		CFDictionaryRef smb_dict,
#endif /* ! TARGET_OS_EMBEDDED */
		CFDictionaryRef dhcp_dict)
{
    CFStringRef			dhcp_key = NULL;
    CFStringRef			dns_key = NULL;
    CFStringRef			ipv4_key = NULL;
#if ! TARGET_OS_EMBEDDED
    CFStringRef			smb_key = NULL;
#endif /* ! TARGET_OS_EMBEDDED */

    /* create the cache keys */
    ipv4_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							   kSCDynamicStoreDomainState,
							   serviceID, 
							   kSCEntNetIPv4);
    dns_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  serviceID, 
							  kSCEntNetDNS);
#if ! TARGET_OS_EMBEDDED
    smb_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  serviceID, 
							  kSCEntNetSMB);
#endif /* ! TARGET_OS_EMBEDDED */
    dhcp_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							   kSCDynamicStoreDomainState,
							   serviceID,
							   kSCEntNetDHCP);
    if (ipv4_key == NULL || dns_key == NULL
#if ! TARGET_OS_EMBEDDED
	|| smb_key == NULL 
#endif /* ! TARGET_OS_EMBEDDED */
	|| dhcp_key == NULL) {
	goto done;
    }

    publish_keys(ipv4_key, ipv4_dict, dns_key, dns_dict, 
#if ! TARGET_OS_EMBEDDED
		 smb_key, smb_dict,
#endif /* ! TARGET_OS_EMBEDDED */
		 dhcp_key, dhcp_dict);
 done:
    my_CFRelease(&ipv4_key);
    my_CFRelease(&dns_key);
#if ! TARGET_OS_EMBEDDED
    my_CFRelease(&smb_key);
#endif /* ! TARGET_OS_EMBEDDED */
    my_CFRelease(&dhcp_key);
    return;
}

static CFDictionaryRef
make_dhcp_dict(Service_t * service_p, absolute_time_t start_time)
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

	if (tag == dhcptag_host_name_e
	    && service_p->method == ipconfig_method_bootp_e) {
	}
	else if (dhcp_parameter_is_ok(tag) == FALSE) {
	    continue;
	}
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
	
	start = CFDateCreate(NULL, (CFAbsoluteTime)start_time);
	CFDictionarySetValue(dict, CFSTR("LeaseStartTime"), start);
	my_CFRelease(&start);
    }

    return (dict);
}

static void
make_link_string(char * string_buffer, const uint8_t * hwaddr, int hwaddr_len)
{
    int		i;
	    
    switch (hwaddr_len) {
    case 6:
	sprintf(string_buffer, EA_FORMAT, EA_LIST(hwaddr));
	break;
    case 8:
	sprintf(string_buffer, FWA_FORMAT, FWA_LIST(hwaddr));
	break;
    default: 
	for (i = 0; i < hwaddr_len; i++) {
	    if (i == 0) {
		sprintf(string_buffer, "%02x", hwaddr[i]);
		string_buffer += 2;
	    }
	    else {
		sprintf(string_buffer, ":%02x", hwaddr[i]);
		string_buffer += 3;
	    }
	}
	break;
    }
    return;
}

static void
dict_insert_router_info(Service_t * service_p, CFMutableDictionaryRef dict)
{
    interface_t *		if_p = service_interface(service_p);
    char			link_string[MAX_LINK_ADDR_LEN * 3];
    CFMutableStringRef		sig_str;

    if (service_router_all_valid(service_p) == FALSE) {
	return;
    }

    sig_str = CFStringCreateMutable(NULL, 0);

    /* router IP address */
    CFStringAppendFormat(sig_str, NULL, CFSTR("IPv4.Router=" IP_FORMAT),
			 IP_LIST(&service_p->router.iaddr));

    /* router link address */
    make_link_string(link_string, service_p->router.hwaddr,
		     if_link_length(if_p));
    CFStringAppendFormat(sig_str, NULL,
			 CFSTR(";IPv4.RouterHardwareAddress=%s"),
			 link_string);

    CFDictionarySetValue(dict, CFSTR("NetworkSignature"), sig_str);
    CFRelease(sig_str);
    return;
}

static void
process_domain_name(const uint8_t * dns_domain, int dns_domain_len,
		    boolean_t search_present, CFMutableDictionaryRef dns_dict)
{
    CFMutableArrayRef	array = NULL;
    int 		i;
    const uint8_t *	name_start = NULL;
    const uint8_t *	scan;

    for (i = 0, scan = dns_domain; i < dns_domain_len; i++, scan++) {
	uint8_t		ch = *scan;

	if (ch == '\0' || isspace(ch)) {
	    if (name_start != NULL) {
		CFStringRef		str;

		if (search_present || ch == '\0') {
		    break;
		}
		str = CFStringCreateWithBytes(NULL, (UInt8 *)name_start,
					      scan - name_start,
					      kCFStringEncodingUTF8, FALSE);
		if (str == NULL) {
		    goto done;
		}
		if (array == NULL) {
		    array = CFArrayCreateMutable(NULL, 0,
						 &kCFTypeArrayCallBacks);
		}
		CFArrayAppendValue(array, str);
		CFRelease(str);
		name_start = NULL;
	    }
	}
	else if (name_start == NULL) {
	    name_start = scan;
	}
    }
    if (name_start != NULL) {
	CFStringRef		str;

	str = CFStringCreateWithBytes(NULL, (UInt8 *)name_start,
				      scan - name_start,
				      kCFStringEncodingUTF8, FALSE);
	if (str == NULL) {
	    goto done;
	}
	if (array == NULL) {
	    CFDictionarySetValue(dns_dict, 
				 kSCPropNetDNSDomainName, str);
	}
	else {
	    CFArrayAppendValue(array, str);
	}
	CFRelease(str);
    }
    if (array != NULL) {
	if (CFArrayGetCount(array) == 1) {
	    CFDictionarySetValue(dns_dict, 
				 kSCPropNetDNSDomainName,
				 CFArrayGetValueAtIndex(array, 0));
	}
	else {
	    CFDictionarySetValue(dns_dict,
				 kSCPropNetDNSSearchDomains, 
				 array);
	}
    }
 done:
    my_CFRelease(&array);
    return;
}

void
service_publish_success(Service_t * service_p, void * pkt, int pkt_size)
{
    service_publish_success2(service_p, pkt, pkt_size, timer_current_secs());
}

void
service_publish_success2(Service_t * service_p, void * pkt, int pkt_size,
			 absolute_time_t start_time)
{
    CFMutableArrayRef		array = NULL;
    CFDictionaryRef		dhcp_dict = NULL;
    CFMutableDictionaryRef	dns_dict = NULL;
    const uint8_t *		dns_domain = NULL;
    int				dns_domain_len = 0;
    struct in_addr *		dns_server = NULL;
    int				dns_server_len = 0;
    uint8_t *			dns_search = NULL;
    int				dns_search_len = 0;
    int				i;
    char *			host_name = NULL;
    int				host_name_len = 0;
    inet_addrinfo_t *		info_p;
    CFMutableDictionaryRef	ipv4_dict = NULL;
    struct completion_results *	pub;
    boolean_t			publish_parent = FALSE;
    struct in_addr *		router = NULL;
#if ! TARGET_OS_EMBEDDED
    CFMutableDictionaryRef	smb_dict = NULL;
    const uint8_t *		smb_nodetype = NULL;
    int				smb_nodetype_len = 0;
    const uint8_t *		smb_scope = NULL;
    int				smb_scope_len = 0;
    struct in_addr *		smb_server = NULL;
    int				smb_server_len = 0;
#endif /* ! TARGET_OS_EMBEDDED */
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
    if (service_p->method == ipconfig_method_bootp_e
	|| dhcp_parameter_is_ok(dhcptag_host_name_e)) {
	host_name = (char *)
	    dhcpol_find(&pub->options, 
			dhcptag_host_name_e,
			&host_name_len, NULL);
    }
    if (dhcp_parameter_is_ok(dhcptag_router_e)) {
	router = (struct in_addr *)
	    dhcpol_find_with_length(&pub->options, dhcptag_router_e,
				    sizeof(*router));
    }
    if (dhcp_parameter_is_ok(dhcptag_domain_name_server_e)) {
	dns_server = (struct in_addr *)
	    dhcpol_find(&pub->options, 
			dhcptag_domain_name_server_e,
			&dns_server_len, NULL);
    }
    if (dhcp_parameter_is_ok(dhcptag_domain_name_e)) {
	dns_domain = (const uint8_t *) 
	    dhcpol_find(&pub->options, 
			dhcptag_domain_name_e,
			&dns_domain_len, NULL);
    }
    /* search can span multiple options, allocate contiguous buffer */
    if (dhcp_parameter_is_ok(dhcptag_domain_search_e)) {
	dns_search = (uint8_t *)
	    dhcpol_get(&pub->options, 
		       dhcptag_domain_search_e,
		       &dns_search_len);
    }
#if ! TARGET_OS_EMBEDDED
    if (dhcp_parameter_is_ok(dhcptag_nb_over_tcpip_name_server_e)) {
	smb_server = (struct in_addr *)
	    dhcpol_find(&pub->options, 
			dhcptag_nb_over_tcpip_name_server_e,
			&smb_server_len, NULL);
    }
    if (dhcp_parameter_is_ok(dhcptag_nb_over_tcpip_node_type_e)) {
	smb_nodetype = (uint8_t *)
	    dhcpol_find(&pub->options, 
			dhcptag_nb_over_tcpip_node_type_e,
			&smb_nodetype_len, NULL);
    }
    if (dhcp_parameter_is_ok(dhcptag_nb_over_tcpip_scope_e)) {
	smb_scope = (uint8_t *)
	    dhcpol_find(&pub->options, 
			dhcptag_nb_over_tcpip_scope_e,
			&smb_scope_len, NULL);
    }
#endif /* ! TARGET_OS_EMBEDDED */
    /* set the router */
    if (router != NULL) {
	CFStringRef		str;

	str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), 
				       IP_LIST(router));
	CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Router, str);
	CFRelease(str);
    }
    /* set the hostname */
    if (host_name && host_name_len > 0) {
	CFStringRef		str;
	str = CFStringCreateWithBytes(NULL, (UInt8 *)host_name,
				      host_name_len,
				      kCFStringEncodingMacRoman, 
				      FALSE);
	if (str != NULL) {
	    CFDictionarySetValue(ipv4_dict, CFSTR("Hostname"), str);
	    CFRelease(str);
	}
    }

    /* insert the signature */
    dict_insert_router_info(service_p, ipv4_dict);

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
	    process_domain_name(dns_domain, dns_domain_len,
				(dns_search != NULL), dns_dict);
	}
	if (dns_search != NULL) {
	    const char * *	search_list = NULL;
	    int			search_count = 0;

	    search_list = DNSNameListCreate(dns_search, dns_search_len,
					    &search_count);
	    if (search_list != NULL) {
		array = CFArrayCreateMutable(NULL, 
					     search_count,
					     &kCFTypeArrayCallBacks);
		for (i = 0; i < search_count; i++) {
		    str = CFStringCreateWithCString(NULL, search_list[i],
						    kCFStringEncodingUTF8);
		    CFArrayAppendValue(array, str);
		    CFRelease(str);
		}
		CFDictionarySetValue(dns_dict, kSCPropNetDNSSearchDomains, 
				     array);
		CFRelease(array);
		free(search_list);
	    }
	}
    }

#if ! TARGET_OS_EMBEDDED
    /* set the SMB information */
    if ((smb_server && smb_server_len >= sizeof(struct in_addr))
	|| (smb_nodetype && smb_nodetype_len == sizeof(uint8_t))
	|| (smb_scope && smb_scope_len > 0)) {
	smb_dict 
	= CFDictionaryCreateMutable(NULL, 0,
				    &kCFTypeDictionaryKeyCallBacks,
				    &kCFTypeDictionaryValueCallBacks);
	if (smb_server && smb_server_len >= sizeof(struct in_addr)) {
	    array = CFArrayCreateMutable(NULL, 
					 smb_server_len / sizeof(struct in_addr),
					 &kCFTypeArrayCallBacks);
	    for (i = 0; i < (smb_server_len / sizeof(struct in_addr)); i++) {
		CFStringRef		str;
		str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
					       IP_LIST(smb_server + i));
		CFArrayAppendValue(array, str);
		CFRelease(str);
	    }
	    CFDictionarySetValue(smb_dict, kSCPropNetSMBWINSAddresses, array);
	    CFRelease(array);
	}
	if (smb_nodetype && smb_nodetype_len == sizeof(uint8_t)) {
	    switch (smb_nodetype[0]) {
	    case 1 :
		CFDictionarySetValue(smb_dict, kSCPropNetSMBNetBIOSNodeType,
				     kSCValNetSMBNetBIOSNodeTypeBroadcast);
		break;
	    case 2 :
		CFDictionarySetValue(smb_dict, kSCPropNetSMBNetBIOSNodeType,
				     kSCValNetSMBNetBIOSNodeTypePeer);
		break;
	    case 4:
		CFDictionarySetValue(smb_dict, kSCPropNetSMBNetBIOSNodeType,
				     kSCValNetSMBNetBIOSNodeTypeMixed);
		break;
	    case 8 :
		CFDictionarySetValue(smb_dict, kSCPropNetSMBNetBIOSNodeType,
				     kSCValNetSMBNetBIOSNodeTypeHybrid);
		break;
	    }
	}
	if (smb_scope && smb_scope_len > 0) {
	    CFStringRef		str;
	    str = CFStringCreateWithBytes(NULL, (UInt8 *)smb_scope,
					  smb_scope_len,
					  kCFStringEncodingUTF8, 
					  FALSE);
	    if (str != NULL) {
		CFDictionarySetValue(smb_dict, kSCPropNetSMBNetBIOSScope, str);
		CFRelease(str);
	    }
	}
    }
#endif /* ! TARGET_OS_EMBEDDED */
    
    /* publish the DHCP options */
    dhcp_dict = make_dhcp_dict(service_p, start_time);

    if (publish_parent) {
	publish_service(service_p->parent_serviceID, 
			ipv4_dict, dns_dict, 
#if ! TARGET_OS_EMBEDDED
			smb_dict, 
#endif /* ! TARGET_OS_EMBEDDED */
			dhcp_dict);
    }
    else {
	publish_service(service_p->serviceID,
			ipv4_dict, dns_dict, 
#if ! TARGET_OS_EMBEDDED
			smb_dict,
#endif /* ! TARGET_OS_EMBEDDED */
			dhcp_dict);
    }
    my_CFRelease(&ipv4_dict);
    my_CFRelease(&dns_dict);
#if ! TARGET_OS_EMBEDDED
    my_CFRelease(&smb_dict);
#endif /* ! TARGET_OS_EMBEDDED */
    my_CFRelease(&dhcp_dict);
    if (dns_search != NULL) {
	free(dns_search);
    }
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

#define N_MIB		6
static int
flush_dynamic_routes(int s)
{
    char *		buf = NULL;
    int			i;
    char *		lim;
    int 		mib[N_MIB];
    size_t 		needed;
    char *		next;
    struct rt_msghdr *	rtm;
    struct sockaddr_in *sin;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_INET;
    mib[4] = NET_RT_FLAGS;
    mib[5] = RTF_DYNAMIC;
    for (i = 0; i < 3; i++) {
	if (sysctl(mib, N_MIB, NULL, &needed, NULL, 0) < 0) {
	    break;
	}
	if ((buf = malloc(needed)) == NULL) {
	    break;
	}
	if (sysctl(mib, N_MIB, buf, &needed, NULL, 0) >= 0) {
	    break;
	}
	free(buf);
	buf = NULL;
    }
    if (buf == NULL) {
	return (-1);
    }
    lim = buf + needed;
    for (next = buf; next < lim; next += rtm->rtm_msglen) {
	rtm = (struct rt_msghdr *)next;
	sin = (struct sockaddr_in *)(rtm + 1);
	
	rtm->rtm_type = RTM_DELETE;
	if (write(s, rtm, rtm->rtm_msglen) < 0) {
	    my_log(LOG_NOTICE,
		   "IPConfiguration: removing dynamic route for %s failed, %s",
		   inet_ntoa(sin->sin_addr),strerror(errno));
	}
	else if (G_IPConfiguration_verbose) {
	    my_log(LOG_NOTICE,
		   "IPConfiguration: removed dynamic route for %s", 
		   inet_ntoa(sin->sin_addr));
	}
    }
    free(buf);
    return (0);
}

static void
flush_routes(const struct in_addr ip, const struct in_addr broadcast) 
{
    int		s;

    s = arp_open_routing_socket();
    if (s < 0) {
	return;
    }

    /* remove permanent arp entries for the IP and IP broadcast */
    if (ip.s_addr) { 
	(void)arp_delete(s, ip, 0);
    }
    if (broadcast.s_addr) { 
	(void)arp_delete(s, broadcast, 0);
    }

    /* blow away all non-permanent arp entries */
    (void)arp_flush(s, FALSE);

    /* flush all dynamic routes */
    (void)flush_dynamic_routes(s);
    close(s);
    return;
}

static int
inet_set_autoaddr(const char * ifname, int val)
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
    flush_routes(G_ip_zeroes, G_ip_zeroes);
    return (inet_set_autoaddr(if_name(service_interface(service_p)), 0));
}

static int
inet_attach_interface(const char * ifname)
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
inet_detach_interface(const char * ifname)
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
    struct {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
	struct sockaddr_in	gway;
    } 				rtmsg;
    int 			sockfd = -1;

    sockfd = arp_open_routing_socket();
    if (sockfd < 0) {
	my_log(LOG_NOTICE, "host_route: open routing socket failed, %s",
	       strerror(errno));
	ret = FALSE;
	goto done;
    }

    memset(&rtmsg, 0, sizeof(rtmsg));
    rtmsg.hdr.rtm_type = cmd;
    rtmsg.hdr.rtm_flags = RTF_UP | RTF_STATIC | RTF_HOST;
    rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = arp_get_next_seq();
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
	int	error = errno;

	switch (error) {
	case ESRCH:
	case EEXIST:
	    my_log(LOG_DEBUG, "host_route: write routing socket failed, %s",
		   strerror(error));
	    break;
	default:
	    my_log(LOG_NOTICE, "host_route: write routing socket failed, %s",
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


static boolean_t
subnet_route(int cmd, struct in_addr gateway, struct in_addr netaddr, 
	     struct in_addr netmask, const char * ifname)
{
    int 			len;
    boolean_t			ret = TRUE;
    int 			rtm_seq;
    struct {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
	struct sockaddr_in	gway;
	struct sockaddr_in	mask;
	struct sockaddr_dl	ifp;
	struct sockaddr_in	ifa;
    } 				rtmsg;
    int 			sockfd = -1;

    sockfd = arp_open_routing_socket();
    if (sockfd < 0) {
	my_log(LOG_NOTICE, "subnet_route: open routing socket failed, %s",
	       strerror(errno));
	ret = FALSE;
	goto done;
    }

    memset(&rtmsg, 0, sizeof(rtmsg));
    rtmsg.hdr.rtm_type = cmd;
    rtmsg.hdr.rtm_flags = RTF_UP | RTF_STATIC | RTF_CLONING;
    rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = rtm_seq = arp_get_next_seq();
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
	/* and the interface address (which is the gateway) */
	rtmsg.ifa.sin_len = sizeof(rtmsg.ifa);
	rtmsg.ifa.sin_family = AF_INET;
	rtmsg.ifa.sin_addr = gateway;
    }
    else {
	/* no ifp/ifa information */
	len -= sizeof(rtmsg.ifp) + sizeof(rtmsg.ifa);
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
	    my_log(LOG_NOTICE, "subnet_route: write routing socket failed, %s",
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

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(u_int32_t) - 1))) : sizeof(u_int32_t))

static int
rt_xaddrs(const char * cp, const char * cplim, struct rt_addrinfo * rtinfo)
{
    int 		i;
    struct sockaddr *	sa;

    bzero(rtinfo->rti_info, sizeof(rtinfo->rti_info));
    for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
	if ((rtinfo->rti_addrs & (1 << i)) == 0) {
	    continue;
	}
	sa = (struct sockaddr *)cp;
	if ((cp + sa->sa_len) > cplim) {
	    return (EINVAL);
	}
	rtinfo->rti_info[i] = sa;
	cp += ROUNDUP(sa->sa_len);
    }
    return (0);
}

static int
subnet_route_if_index(struct in_addr netaddr, struct in_addr netmask)
{
    struct sockaddr_in *	addr_p;
    int 			len;
    int				n;
    char *			ptr;
    int				pid = getpid();
    boolean_t			ret_if_index = 0;
    int 			rtm_seq;
    struct {
	struct rt_msghdr hdr;
	char		 data[512];
    }				rtmsg;
    struct sockaddr_dl *	sdl;
    int 			sockfd = -1;

    sockfd = arp_open_routing_socket();
    if (sockfd < 0) {
	my_log(LOG_NOTICE, 
	       "subnet_route_if_index: open routing socket failed, %s",
	       strerror(errno));
	goto done;
    }
    memset(&rtmsg, 0, sizeof(rtmsg));
    rtmsg.hdr.rtm_type = RTM_GET_SILENT;
    rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = rtm_seq = arp_get_next_seq();
    rtmsg.hdr.rtm_addrs = RTA_DST | RTA_NETMASK;
    ptr = rtmsg.data;

    /* RTA_DST */
    addr_p = (struct sockaddr_in *)ptr;
    addr_p->sin_len = sizeof(*addr_p);
    addr_p->sin_family = AF_INET;
    addr_p->sin_addr = netaddr;
    ptr += sizeof(*addr_p);

    /* RTA_NETMASK */
    addr_p = (struct sockaddr_in *)ptr;
    addr_p->sin_len = sizeof(*addr_p);
    addr_p->sin_family = AF_INET;
    addr_p->sin_addr = netmask;
    ptr += sizeof(*addr_p);

    len = ptr - (char *)&rtmsg;
    rtmsg.hdr.rtm_msglen = len;
    if (write(sockfd, &rtmsg, len) < 0) {
	int	error = errno;

	switch (error) {
	case ESRCH:
	case EEXIST:
	    my_log(LOG_DEBUG,
		   "subnet_route_if_index: write routing socket failed, %s",
		   strerror(error));
	    break;
	default:
	    my_log(LOG_NOTICE,
		   "subnet_route_if_index: write routing socket failed, %s",
		   strerror(error));
	    break;
	}
	goto done;
    }
    while ((n = read(sockfd, &rtmsg, sizeof(rtmsg))) > 0) {
	struct rt_addrinfo	info;

	if (rtmsg.hdr.rtm_type != RTM_GET
	    || rtmsg.hdr.rtm_seq != rtm_seq || rtmsg.hdr.rtm_pid != pid) {
	    continue;
	}
	info.rti_addrs = rtmsg.hdr.rtm_addrs;
	if (rt_xaddrs(rtmsg.data, rtmsg.data + n, &info) != 0) {
	    syslog(LOG_DEBUG, "subnet_route_if_index: rt_xaddrs failed");
	    break;
	}
	sdl = (struct sockaddr_dl *)info.rti_info[RTAX_GATEWAY];
	if (sdl == NULL || sdl->sdl_family != AF_LINK) {
	    syslog(LOG_DEBUG,
		   "subnet_route_if_index: can't get interface name");
	    break;
	}
	ret_if_index = sdl->sdl_index;
	break;
    }
 done:
    if (sockfd >= 0) {
	close(sockfd);
    }
    return (ret_if_index);
}

static boolean_t
subnet_route_add(struct in_addr gateway, struct in_addr netaddr, 
		 struct in_addr netmask, const char * ifname)
{
    return (subnet_route(RTM_ADD, gateway, netaddr, netmask, ifname));
}

static boolean_t
subnet_route_delete(struct in_addr gateway, struct in_addr netaddr, 
		    struct in_addr netmask)
{
    return (subnet_route(RTM_DELETE, gateway, netaddr, netmask, NULL));
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
	int count = CFArrayGetCount(arr);

	for (i = 0; i < count; i++) {
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
 * Function: service_should_do_router_arp
 * Purpose:
 *   Check whether we should be doing router arp or not.  If the interface
 *   is wireless, check the current SSID to see if it is on the list of
 *   excluded SSID's.
 * Returns:
 *   TRUE if router ARP is OK, FALSE otherwise.
 */
boolean_t
service_should_do_router_arp(Service_t * service_p)
{
    IFState_t *		ifstate = service_ifstate(service_p);
    CFStringRef		ssid = ifstate->ssid;

    if (if_is_wireless(ifstate->if_p) == FALSE) {
	/* non-wireless interfaces are always enabled */
	return (TRUE);
    }
    if (S_router_arp_excluded_ssids == NULL
	|| S_router_arp_excluded_ssids_range.length == 0) {
	/* there are no excluded SSIDs */
	return (TRUE);
    }
    if (ssid == NULL) {
	/* we don't know the SSID, assume no */
	return (FALSE);
    }
    if (CFArrayContainsValue(S_router_arp_excluded_ssids, 
			     S_router_arp_excluded_ssids_range, ssid)) {
	/* SSID is explicitly excluded */
	if (G_IPConfiguration_verbose) {
	    SCLog(TRUE, LOG_NOTICE,
		  CFSTR("SSID '%@' is in the Router ARP exclude list"),
		  ssid);
	}
	return (FALSE);
    }
    return (TRUE);

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
    IFState_t *			ifstate = service_ifstate(parent_service_p);
    Service_t *			ll_parent_p;
    boolean_t			needs_stop;

    if (IFStateList_linklocal_service(&S_ifstate_list, NULL) != NULL) {
	/* don't touch user-configured link-local services */
	return;
    }
    ll_parent_p = service_parent_service(ifstate->linklocal_service_p);
    if (ll_parent_p == NULL) {
	S_linklocal_needs_attention = TRUE;
	return;
    }
    if (parent_service_p != ll_parent_p) {
	/* we're not the one that triggered the link-local service */
	S_linklocal_needs_attention = TRUE;
	return;
    }
    bzero(&data, sizeof(data));
    data.reserved_0 = no_allocate;
    (void)config_method_change(ifstate->linklocal_service_p,
			       ipconfig_method_linklocal_e,
			       &data, sizeof(data), &needs_stop);
    return;
}

void
linklocal_set_needs_attention()
{
    S_linklocal_needs_attention = TRUE;
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
	my_log(LOG_NOTICE,
	       "IPConfiguration: failed to start link-local service on %s, %s",
	       if_name(ifstate->if_p),
	       ipconfig_status_string(status));
    }
    return;
}

/*
 * Function: S_linklocal_elect
 * Purpose:
 */
static void
S_linklocal_elect(CFArrayRef service_order)
{
    int 		i;
    unsigned int	ll_best_rank = RANK_NONE;
    Service_t *		ll_best_service_p = NULL;
    struct in_addr	mask;
    struct in_addr	netaddr;

    if (IFStateList_linklocal_service(&S_ifstate_list, NULL) != NULL) {
	/* don't touch user-configured link-local services */
	return;
    }

    for (i = 0; i < dynarray_count(&S_ifstate_list); i++) {
	unsigned int	best_rank = RANK_NONE;
	Service_t *	best_service_p = NULL;
	IFState_t *	ifstate = dynarray_element(&S_ifstate_list, i);
	int		j;
	Service_t * 	ll_parent_p = NULL;
	unsigned int	rank;

	if (ifstate->free_in_progress == TRUE) {
	    /* this can't happen */
	    continue;
	}
	if (ifstate->linklocal_service_p != NULL) {
	    ll_parent_p = service_parent_service(ifstate->linklocal_service_p);
	    if (ll_parent_p == NULL) {
		/* the parent of the link-local service is gone */
		IFState_service_free(ifstate,
				     ifstate->linklocal_service_p->serviceID);
		/* side-effect: ifstate->linklocal_service_p = NULL */
	    }
	}
	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    Service_t * 		service_p;
	    inet_addrinfo_t *		info_p;

	    service_p = dynarray_element(&ifstate->services, j);
	    if (service_p->free_in_progress == TRUE) {
		/* this can't happen */
		continue;
	    }
	    if (service_p->method == ipconfig_method_linklocal_e) {
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
	    if (rank < best_rank
		|| (best_service_p != NULL
		    && best_service_p->info.addr.s_addr == 0
		    && info_p->addr.s_addr != 0)) {
		best_service_p = service_p;
		best_rank = rank;
	    }
	}
	if (ll_parent_p != best_service_p) {
	    if (ll_parent_p != NULL) {
		my_CFRelease(&ll_parent_p->child_serviceID);
		IFState_service_free(ifstate,
				     ifstate->linklocal_service_p->serviceID);
	    }
	    if (best_service_p != NULL) {
		boolean_t	no_allocate = TRUE;

		if (best_service_p->info.addr.s_addr == 0) {
		    no_allocate = FALSE;
		}
		S_linklocal_start(best_service_p, no_allocate);
	    }
	}
	if (best_service_p != NULL) {
	    rank = S_get_service_rank(service_order, best_service_p);
	    if (rank < ll_best_rank) {
		ll_best_service_p = ll_parent_p;
		ll_best_rank = rank;
	    }
	}
    }

    /* remove or set the 169.254/16 subnet */
    mask.s_addr = htonl(IN_CLASSB_NET);
    netaddr.s_addr = htonl(IN_LINKLOCALNETNUM);
    if (ll_best_service_p == NULL) {
	my_log(LOG_DEBUG, "deleting subnet for 169.254/16");
	subnet_route_delete(G_ip_zeroes, netaddr, mask);
    }
    else {
	int		if_index;
	interface_t *	route_if_p = NULL;

	/* check which interface the 169.254/16 subnet is currently on */
	if_index = subnet_route_if_index(netaddr, mask);
	if (if_index != 0) {
	    route_if_p = ifl_find_link(S_interfaces, if_index);
	}
	if (route_if_p == NULL 
	    || strcmp(if_name(route_if_p), 
		      if_name(service_interface(ll_best_service_p))) != 0) {
	    subnet_route_delete(G_ip_zeroes, netaddr, mask);
	    subnet_route_add(ll_best_service_p->info.addr, 
			     netaddr, mask, 
			     if_name(service_interface(ll_best_service_p)));
	}
	else {
	    my_log(LOG_DEBUG, "subnet for 169.254/16 still good on interface %s",
		   if_name(route_if_p));
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
    interface_t *	if_p = service_interface(service_p);
    int			ret = 0;
    struct in_addr	netaddr = { 0 };
    int 		s = inet_dgram_socket();

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
    flush_routes(G_ip_zeroes, broadcast);
    S_linklocal_needs_attention = TRUE;
    return (ret);
}

static int
S_remove_ip_address(const char * ifname, struct in_addr this_ip)
{
    int			ret = 0;
    int 		s;

    s = inet_dgram_socket();
    my_log(LOG_DEBUG, "S_remove_ip_address(%s) " IP_FORMAT, 
	   ifname, IP_LIST(&this_ip));
    if (s < 0) {
	ret = errno;
	my_log(LOG_DEBUG, 
	       "S_remove_ip_address(%s) socket() failed, %s (%d)",
	       ifname, strerror(errno), errno);
    }	
    else { 
	if (inet_difaddr(s, ifname, &this_ip) < 0) {
	    ret = errno;
	    my_log(LOG_DEBUG, "S_remove_ip_address(%s) " 
		   IP_FORMAT " failed, %s (%d)", ifname,
		   IP_LIST(&this_ip), strerror(errno), errno);
	}
	close(s);
    }
    return (ret);
}

int
service_remove_address(Service_t * service_p)
{
    interface_t *	if_p = service_interface(service_p);
    inet_addrinfo_t *	info_p = &service_p->info;
    int			ret = 0;

    if (info_p->addr.s_addr != 0) {
	inet_addrinfo_t		saved_info;

	/* copy IP info then clear it so that it won't be elected */
	saved_info = service_p->info;
	bzero(info_p, sizeof(*info_p));

	/* if no service on this interface refers to this IP, remove the IP */
	if (IFState_service_with_ip(service_ifstate(service_p),
				    saved_info.addr) == NULL) {
	    /*
	     * This can only happen if there's a manual/inform service 
	     * and a BOOTP/DHCP service with the same IP.  Duplicate
	     * manual/inform services are prevented when created.
	     */
	    ret = S_remove_ip_address(if_name(if_p), saved_info.addr);
	}
	/* if no service refers to this IP, remove the host route for the IP */
	if (IFStateList_service_with_ip(&S_ifstate_list, 
					saved_info.addr, NULL) == NULL) {
	    (void)host_route(RTM_DELETE, saved_info.addr);
	}
	flush_routes(saved_info.addr, saved_info.broadcast);
    }
    S_linklocal_needs_attention = TRUE;
    return (ret);
}

static void
set_loopback()
{
    struct in_addr	loopback;
    struct in_addr	loopback_net;
    struct in_addr	loopback_mask;
    int 		s = inet_dgram_socket();

#ifndef INADDR_LOOPBACK_NET
#define	INADDR_LOOPBACK_NET		(u_int32_t)0x7f000000
#endif	INADDR_LOOPBACK_NET

    loopback.s_addr = htonl(INADDR_LOOPBACK);
    loopback_mask.s_addr = htonl(IN_CLASSA_NET);
    loopback_net.s_addr = htonl(INADDR_LOOPBACK_NET);

    if (s < 0) {
	my_log(LOG_ERR, 
	       "set_loopback(): socket() failed, %s (%d)",
	       strerror(errno), errno);
	return;
    }
    if (inet_aifaddr(s, "lo0", &loopback, &loopback_mask, NULL) < 0) {
	my_log(LOG_DEBUG, "set_loopback: inet_aifaddr() failed, %s (%d)", 
	       strerror(errno), errno);
    }
    close(s);

    /* add 127/8 route */
    if (subnet_route_add(loopback, loopback_net, loopback_mask, "lo0")
	== FALSE) {
	my_log(LOG_DEBUG, "set_loopback: subnet_route_add() failed, %s (%d)", 
	       strerror(errno), errno);
    }
    return;
}

/**
 ** Routines for MiG interface
 **/

static FILE *
logfile_fopen(const char * path)
{
    FILE *	f;

    f = fopen(path, "a");
    if (f == NULL) {
	my_log(LOG_DEBUG, "logfile_fopen fopen '%s' failed, %s",
	       path, strerror(errno));
	return (NULL);
    }
    my_log(LOG_DEBUG, "IPConfiguration logfile '%s' opened", path);
    return (f);
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


ipconfig_status_t
set_verbose(int verbose)
{
    my_log(LOG_NOTICE, "IPConfiguration: verbose mode %s",
	   verbose ? "enabled" : "disabled");
    G_IPConfiguration_verbose = verbose;
    if (verbose == 0) {
	if (S_IPConfiguration_log_file != NULL) {
	    (void)fclose(S_IPConfiguration_log_file);
	    S_IPConfiguration_log_file = NULL;
	}
    }
    else {
	if (S_IPConfiguration_log_file == NULL) {
	    S_IPConfiguration_log_file 
		= logfile_fopen(IPCONFIGURATION_BOOTP_LOG);
	}
    }
    bootp_session_set_debug(G_bootp_session, S_IPConfiguration_log_file);
    return (ipconfig_status_success_e);
}

int
get_if_count()
{
    return (dynarray_count(&S_ifstate_list));
}

boolean_t 
get_if_name(int intface, char * name, size_t namelen)
{
    boolean_t		ret = FALSE;
    IFState_t * 	s;

    s = dynarray_element(&S_ifstate_list, intface);
    if (s) {
	strlcpy(name, if_name(s->if_p), namelen);
	ret = TRUE;
    }
    return (ret);
}

boolean_t 
get_if_addr(const char * name, u_int32_t * addr)
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
get_if_option(const char * name, int option_code, void * option_data, 
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
get_if_packet(const char * name, void * packet_data, unsigned int * packet_dataCnt)
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
wait_if(const char * name)
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

      case ipconfig_method_failover_e: {
	  return failover_thread;
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
    int				type = if_ift_type(if_p);

    switch (type) {
    case IFT_ETHER:
    case IFT_IEEE1394:
    case IFT_L2VLAN:
    case IFT_IEEE8023ADLAG:
	break;
    default:
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
if_gifmedia(int sockfd, const char * name, boolean_t * status)
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
	SCLog(TRUE, LOG_NOTICE, 
	      CFSTR("config_method_event(%s): lookup_func(%d) failed"), 
	      IFEventID_names(event), method);
	status = ipconfig_status_internal_error_e;
	goto done;
    }
    (*func)(service_p, event, data);

 done:
    return (status);
    
}

static void
all_services_event(IFStateList_t * list, IFEventID_t event)
{
    int 		i;
    int			if_count = dynarray_count(list);

    for (i = 0; i < if_count; i++) {
	IFState_t *		ifstate = dynarray_element(list, i);
	int			j;
	int			service_count;

	service_count = dynarray_count(&ifstate->services);
	for (j = 0; j < service_count; j++) {
	    Service_t * service_p = dynarray_element(&ifstate->services, j);
	    (void)config_method_event(service_p, event, NULL);
	}
    }
    return;
}

static ipconfig_status_t
config_method_stop(Service_t * service_p)
{
    return (config_method_event(service_p, IFEventID_stop_e, NULL));
}

static ipconfig_status_t
config_method_media(Service_t * service_p, boolean_t network_changed)
{
    /* if there's a media event, we need to re-ARP */
    service_router_clear_arp_verified(service_p);
    return (config_method_event(service_p, IFEventID_media_e, 
				(void *)(intptr_t)network_changed));
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
    /* renew forces a re-ARP too */
    service_router_clear_arp_verified(service_p);
    return (config_method_event(service_p, IFEventID_renew_e, NULL));
}

ipconfig_status_t
set_if(const char * name, ipconfig_method_t method,
       void * method_data, unsigned int method_data_len)
{
    interface_t * 	if_p = ifl_find_name(S_interfaces, name);
    IFState_t *   	ifstate;

    if (G_IPConfiguration_verbose)
	my_log(LOG_NOTICE, "set %s %s", name, ipconfig_method_string(method));
    if (if_p == NULL) {
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
    return (IFState_service_add(ifstate, NULL, method, method_data,
				method_data_len, NULL, NULL));
}

static CFStringRef
myCFUUIDStringCreate(CFAllocatorRef alloc)
{
    CFUUIDRef 	uuid;
    CFStringRef	uuid_str;

    uuid = CFUUIDCreate(alloc);
    uuid_str = CFUUIDCreateString(alloc, uuid);
    CFRelease(uuid);
    return (uuid_str);
}

static ipconfig_status_t
add_or_set_service(const char * name, ipconfig_method_t method, 
		   bool add_only,
		   void * method_data, unsigned int method_data_len,
		   void * service_id, unsigned int * service_id_len)
{
    interface_t * 	if_p = ifl_find_name(S_interfaces, name);
    IFState_t *   	ifstate;
    unsigned int	in_length;
    Service_t *		service_p;
    CFStringRef		serviceID;
    ipconfig_status_t	status;

    in_length = *service_id_len;
    *service_id_len = 0;
    if (method == ipconfig_method_none_e) {
	return (ipconfig_status_invalid_parameter_e);
    }
    if (if_p == NULL) {
	return (ipconfig_status_interface_does_not_exist_e);
    }
    ifstate = IFStateList_ifstate_create(&S_ifstate_list, if_p);
    if (ifstate == NULL) {
	return (ipconfig_status_allocation_failed_e);
    }
    service_p = IFState_service_matching_method(ifstate, method,
						method_data, method_data_len,
						FALSE);
    if (service_p != NULL) {
	boolean_t	needs_stop = FALSE;

	if (add_only) {
	    return (ipconfig_status_duplicate_service_e);
	}
	status = config_method_change(service_p, method,
				      method_data, method_data_len, 
				      &needs_stop);
	if (status == ipconfig_status_success_e
	    && needs_stop == FALSE) {
	    return (ipconfig_status_success_e);
	}
	IFState_service_free(ifstate, service_p->serviceID);
    }
    serviceID = myCFUUIDStringCreate(NULL);
    if (serviceID == NULL) {
	return (ipconfig_status_allocation_failed_e);
    }
    /* add a new service */
    if (G_IPConfiguration_verbose) {
	my_log(LOG_NOTICE, "%s %s %s", add_only ? "add_service" : "set_service",
	       name, ipconfig_method_string(method));
    }
    status = IFState_service_add(ifstate, serviceID, method, method_data,
				 method_data_len, NULL, &service_p);
    if (status == ipconfig_status_success_e) {
	CFIndex		len = 0;
	service_p->is_dynamic = TRUE;
	(void)CFStringGetBytes(serviceID,
			       CFRangeMake(0, CFStringGetLength(serviceID)),
			       kCFStringEncodingASCII,
			       0, FALSE, service_id, in_length,
			       &len);
	*service_id_len = len;
    }
    CFRelease(serviceID);
    return (status);
}

__private_extern__ ipconfig_status_t
add_service(const char * name, ipconfig_method_t method,
	    void * method_data, unsigned int method_data_len,
	    void * service_id, unsigned int * service_id_len)
{
    return (add_or_set_service(name, method, TRUE, method_data,
			       method_data_len, service_id, service_id_len));
}

__private_extern__ ipconfig_status_t
set_service(const char * name, ipconfig_method_t method,
	    void * method_data, unsigned int method_data_len,
	    void * service_id, unsigned int * service_id_len)
{
    return (add_or_set_service(name, method, FALSE, method_data,
			       method_data_len, service_id, service_id_len));
}

__private_extern__ ipconfig_status_t
remove_service_with_id(void * service_id, unsigned int service_id_len)
{
    IFState_t *   	ifstate;
    CFStringRef		serviceID;
    Service_t *		service_p;
    ipconfig_status_t	status;

    serviceID = CFStringCreateWithBytes(NULL, service_id, service_id_len,
					kCFStringEncodingASCII, FALSE);
    if (serviceID == NULL) {
	return (ipconfig_status_allocation_failed_e);
    }
    ifstate = IFStateList_service_with_ID(&S_ifstate_list,
					  serviceID, &service_p);
    if (ifstate == NULL) {
	status = ipconfig_status_no_such_service_e;
	goto done;
    }
    if (service_p->is_dynamic == FALSE) {
	status = ipconfig_status_invalid_operation_e;
	goto done;
    }
    if (G_IPConfiguration_verbose) {
	my_log(LOG_NOTICE, "remove_service %s %s", if_name(ifstate->if_p),
	       ipconfig_method_string(service_p->method));
    }
    /* remove the service */
    IFState_service_free(ifstate, serviceID);
    status = ipconfig_status_success_e;

 done:
    CFRelease(serviceID);
    return (status);
}

__private_extern__ ipconfig_status_t
find_service(const char * name, boolean_t exact,
	     ipconfig_method_t method,
	     void * method_data, unsigned int method_data_len,
	     void * service_id, unsigned int * service_id_len)
{
    IFState_t *   	ifstate;
    unsigned int	in_length;
    CFIndex		len = 0;
    Service_t *		service_p;

    in_length = *service_id_len;
    *service_id_len = 0;
    if (method == ipconfig_method_none_e) {
	return (ipconfig_status_invalid_parameter_e);
    }
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, name, NULL);
    if (ifstate == NULL) {
	return (ipconfig_status_interface_does_not_exist_e);
    }
    if (exact) {
	service_p 
	    = IFState_service_with_method(ifstate, method,
					  method_data, method_data_len,
					  FALSE);
    }
    else {
	service_p
	    = IFState_service_matching_method(ifstate, method,
					      method_data, method_data_len,
					      FALSE);
    }
    if (service_p == NULL) {
	return (ipconfig_status_no_such_service_e);
    }
    (void)CFStringGetBytes(service_p->serviceID,
			   CFRangeMake(0, 
				       CFStringGetLength(service_p->serviceID)),
			   kCFStringEncodingASCII,
			   0, FALSE, service_id, in_length,
			   &len);
    *service_id_len = len;
    return (ipconfig_status_success_e);
}

__private_extern__ ipconfig_status_t
remove_service(const char * name, ipconfig_method_t method,
	       void * method_data, unsigned int method_data_len)
{
    IFState_t *   	ifstate;
    Service_t *		service_p;

    if (method == ipconfig_method_none_e) {
	return (ipconfig_status_invalid_parameter_e);
    }
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, name, NULL);
    if (ifstate == NULL) {
	return (ipconfig_status_interface_does_not_exist_e);
    }
    service_p = IFState_service_with_method(ifstate, method,
					    method_data, method_data_len,
					    FALSE);
    if (service_p == NULL) {
	return (ipconfig_status_no_such_service_e);
    }
    if (service_p->is_dynamic == FALSE) {
	return (ipconfig_status_invalid_operation_e);
    }
    if (G_IPConfiguration_verbose) {
	my_log(LOG_NOTICE, "remove_service %s %s", if_name(ifstate->if_p),
	       ipconfig_method_string(service_p->method));
    }
    /* remove the service */
    IFState_service_free(ifstate, service_p->serviceID);
    return (ipconfig_status_success_e);
}

static boolean_t
get_media_status(const char * name, boolean_t * media_status) 

{
    boolean_t	media_valid = FALSE;
    int		sockfd;
	    
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	my_log(LOG_NOTICE, "get_media_status (%s): socket failed, %s", 
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
    else if (CFEqual(m, kSCValNetIPv4ConfigMethodFailover)) {
	*method = ipconfig_method_failover_e;
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
    CFBooleanRef		b;
    CFStringRef			config_method;
    int				count = 0;
    CFStringRef			client_id = NULL;
    char			cid[255];
    int				cid_len = 0;
    int				i;
    boolean_t			ignore_link_status;
    CFArrayRef			masks = NULL;
    ipconfig_method_data_t *	method_data = NULL;
    int				method_data_len = 0;
    CFStringRef			router = NULL;

    config_method = CFDictionaryGetValue(dict, 
					 kSCPropNetIPv4ConfigMethod);
    if (config_method == NULL 
	|| ipconfig_method_from_cfstring(config_method, method) == FALSE) {
	my_log(LOG_ERR, "ipconfigd: configuration method is missing/invalid");
	goto error;
    }
    b = isA_CFBoolean(CFDictionaryGetValue(dict, kSCPropNetIgnoreLinkStatus));
    ignore_link_status = (b != NULL) ? CFBooleanGetValue(b) : FALSE;
    router = CFDictionaryGetValue(dict, kSCPropNetIPv4Router);
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
	if (count > 1) {
	    my_log(LOG_NOTICE, 
		   "ipconfigd: multiple IP addresses - ignoring extras");
	    count = 1;
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
    	case ipconfig_method_failover_e:
	case ipconfig_method_manual_e:
	    if (addresses == NULL || masks == NULL) {
		my_log(LOG_ERR, 
		       "ipconfigd: method requires address and mask");
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
    if (ignore_link_status) {
	method_data->flags |= ipconfig_method_data_flags_ignore_link_status_e;
    }
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
    if (*method == ipconfig_method_manual_e && router != NULL) {
	method_data->u.manual_router = cfstring_to_ip(router);
	if (G_debug) {
	    printf("Router " IP_FORMAT "\n", 
		   IP_LIST(&method_data->u.manual_router));
	}
    }
    if (cid_len > 0) {
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

static CFArrayRef
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
    int count;
    int i;

    if (order == NULL)
	goto done;

    count = CFArrayGetCount(order);
    for (i = 0; i < count; i++) {
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
    int				service_IDs_count;
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

    service_IDs_count = CFArrayGetCount(service_IDs);

    /* sort the list according to the defined service order */
    order_array = get_order_array_from_values(values, order_key);
    if (order_array != NULL && service_IDs_count > 0) {
	CFRange range =  CFRangeMake(0, service_IDs_count);
	
	CFArraySortValues(service_IDs, range, compare_serviceIDs, 
			  (void *)order_array);
    }

    /* populate all_services array with annotated IPv4 dict's */
    for (i = 0; i < service_IDs_count; i++) {
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
	    || (CFEqual(type, kSCValNetInterfaceTypeEthernet) == FALSE
		&& CFEqual(type, kSCValNetInterfaceTypeFireWire) == FALSE)
	    ) {
	    /* we only configure ethernet/firewire interfaces currently */
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
    int			count;
    int 		i;

    if (all == NULL)
	return (NULL);

    count = CFArrayGetCount(all);
    for (i = 0; i < count; i++) {
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
    int			count;
    int 		i;
    CFMutableArrayRef	list = NULL;

    if (all == NULL) {
	return (NULL);
    }

    list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (list == NULL) {
	return (NULL);
    }
    count = CFArrayGetCount(all);
    for (i = 0; i < count; i++) {
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
      case ipconfig_method_failover_e:
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

static Service_t *
find_dynamic_service(const char * ifname, ipconfig_method_t method,
		     ipconfig_method_data_t * method_data,
		     int method_data_len)
{
    interface_t * 	if_p = ifl_find_name(S_interfaces, ifname);
    IFState_t *		ifstate = NULL;

    if (if_p == NULL) {
	return (NULL);
    }
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, ifname, NULL);
    if (ifstate == NULL) {
	return (NULL);
    }
    return (IFState_service_matching_method(ifstate, method,
					    method_data, method_data_len,
					    TRUE));
}

static ServiceConfig_t *
ServiceConfig_list_init(CFArrayRef all_ipv4, const char * ifname, int * count_p)
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
	boolean_t		duplicate_config = FALSE;
	boolean_t		duplicate_dynamic = FALSE;
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
	duplicate_config
	    = (ServiceConfig_list_lookup_method(config_list, count, method, 
						method_data, method_data_len)
	       != NULL);
	if (duplicate_config == FALSE) {
	    duplicate_dynamic
		= (find_dynamic_service(ifname, method, 
					method_data, method_data_len) != NULL);
	}
	if (duplicate_config || duplicate_dynamic) {
	    boolean_t	is_manual = ipconfig_method_is_manual(method);

	    if (is_manual) {
		my_log(LOG_NOTICE, "%s: %s " IP_FORMAT " %s",
		       ifname, ipconfig_method_string(method),
		       IP_LIST(&method_data->ip[0].addr),
		       duplicate_config 
		       ? "duplicate configured service" 
		       : "configured service conflicts with dynamic service");
	    }
	    else {
		my_log(LOG_NOTICE, "%s: %s %s",
		       ifname, ipconfig_method_string(method),
		       duplicate_config 
		       ? "duplicate configured service" 
		       : "configured service conflicts with dynamic service");

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
free_inactive_services(const char * ifname, ServiceConfig_t * config_list,
		       int count)
{
    int			j;
    IFState_t *		ifstate;
    CFMutableArrayRef	list = NULL;
    int			list_count;

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

	if (service_p->is_dynamic) {
	    /* dynamically created services survive configuration changes */
	    continue;
	}
	if (service_p->parent_serviceID != NULL) {
	    /* this service gets cleaned up on its own */
	    continue;
	}
	if (ServiceConfig_list_lookup_service(config_list, count,
					      serviceID) == NULL) {
	    CFArrayAppendValue(list, serviceID);
	}
    }

    list_count = CFArrayGetCount(list);
    for (j = 0; j < list_count; j++) {
	CFStringRef serviceID = CFArrayGetValueAtIndex(list, j);

	IFState_service_free(ifstate, serviceID);
    }

 done:
    my_CFRelease(&list);
    return;
}

static ipconfig_status_t
S_set_service(IFState_t * ifstate, ServiceConfig_t * config)
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
		(void)S_set_service(ifstate, config);
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

    /* notify for a refresh configuration request */
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							kSCCompAnyRegex,
							kSCEntNetRefreshConfiguration);
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

    /* notify when ComputerName/LocalHostName changes */
    S_computer_name_key = SCDynamicStoreKeyCreateComputerName(NULL);
    CFArrayAppendValue(keys, S_computer_name_key);
    S_hostnames_key = SCDynamicStoreKeyCreateHostNames(NULL);
    CFArrayAppendValue(keys, S_hostnames_key);

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
    int			count = dynarray_count(&S_ifstate_list);
    const char * *	names = NULL;
    int			names_count = 0;
    int 		i;


    if (count == 0) {
	return;
    }

    /* allocate worst case scenario in which each ifstate needs to be removed */
    names = (const char * *)malloc(sizeof(char *) * count);
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

    if (S_linklocal_needs_attention == FALSE) {
	return;
    }
    S_linklocal_needs_attention = FALSE;
    if (S_scd_session == NULL) {
	return;
    }
    service_order = S_get_service_order(S_scd_session);
    if (G_IPConfiguration_verbose) {
	my_log(LOG_NOTICE, "before_blocking: calling S_linklocal_elect");
    }
    S_linklocal_elect(service_order);
    my_CFRelease(&service_order);
    
    return;
}

/*
 * Function: update_hibernate_state, woke_from_hibernation
 *
 * Purpose:
 *   When we wake from sleep, check whether we woke from a hibernation
 *   image or a regular wake from sleep.
 */
static uint32_t	S_hibernate_state;

#define IO_PATH_PM_ROOT_DOMAIN kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain"
__private_extern__ void
update_hibernate_state(void)
{
    CFDataRef		hib_prop;

    S_hibernate_state = kIOHibernateStateInactive;
    hib_prop = myIORegistryEntryCopyProperty(IO_PATH_PM_ROOT_DOMAIN,
					     CFSTR(kIOHibernateStateKey)); 
    if (isA_CFData(hib_prop) != NULL 
	&& CFDataGetLength(hib_prop) == sizeof(S_hibernate_state)) {
	S_hibernate_state = *((uint32_t *)CFDataGetBytePtr(hib_prop));
    }
    my_CFRelease(&hib_prop);
    return;
}

boolean_t
woke_from_hibernation(void)
{
    return (S_hibernate_state == kIOHibernateStateWakingFromHibernate);
}

static void 
power_changed(void * refcon, io_service_t service, natural_t msg_type,
	      void * msg)
{
    boolean_t	ack_msg = TRUE;

    switch (msg_type) {
    case kIOMessageSystemWillPowerOff:
    case kIOMessageSystemWillRestart:
	/* 
	 * Note: we never see these messages because we get killed
	 * off before that would happen (SIGTERM, SIGKILL).
	 */
	break;

    case kIOMessageSystemWillNotSleep:
    case kIOMessageSystemWillNotPowerOff:
	ack_msg = FALSE;
	break;

    case kIOMessageSystemWillSleep:
	/* sleep */
	my_log(LOG_DEBUG, "IPConfiguration: Sleep");
	all_services_event(&S_ifstate_list, IFEventID_sleep_e);
	break;

    case kIOMessageSystemWillPowerOn:
	my_log(LOG_DEBUG, "IPConfiguration: Wake");
	update_hibernate_state();
	all_services_event(&S_ifstate_list, IFEventID_wake_e);
	break;

    case kIOMessageSystemHasPoweredOn:
	/* wake */
	ack_msg = FALSE;
	break;

    default:
	break;
    }
    if (ack_msg) {
	IOAllowPowerChange(S_power_connection, (long)msg);
    }
    return;
}


static io_connect_t
power_notification_init()
{
    io_object_t 		obj;
    CFRunLoopSourceRef 		rls;
    IONotificationPortRef 	port;
    io_connect_t 		power_connection;

    power_connection = IORegisterForSystemPower(NULL, &port,
						power_changed, &obj);
    if (power_connection != 0) {
        rls = IONotificationPortGetRunLoopSource(port);
        CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    }
    return (power_connection);
}

#if ! TARGET_OS_EMBEDDED
#define DISK_AND_NET	(kIOPMSystemPowerStateCapabilityDisk \
			 | kIOPMSystemPowerStateCapabilityNetwork)
void 
new_power_changed(void * param, 
		  IOPMConnection connection,
		  IOPMConnectionMessageToken token, 
		  IOPMSystemPowerStateCapabilities capabilities)
{
    IOReturn                ret;

    if ((capabilities & kIOPMSystemPowerStateCapabilityCPU) != 0) {
	if ((capabilities & DISK_AND_NET) != DISK_AND_NET) {
	    /* wait until disk and network are there */
	}
	else if (S_awake == FALSE) {
	    /* wake */
	    S_awake = TRUE;
	    my_log(LOG_DEBUG, "IPConfiguration: Wake");
	    update_hibernate_state();
	    all_services_event(&S_ifstate_list, IFEventID_wake_e);
	}
    }
    else {
	/* sleep */
	S_awake = FALSE;
	my_log(LOG_DEBUG, "IPConfiguration: Sleep");
	all_services_event(&S_ifstate_list, IFEventID_sleep_e);

    }
    ret = IOPMConnectionAcknowledgeEvent(connection, token);    
    if (ret != kIOReturnSuccess) {
	my_log(LOG_NOTICE, "IPConfiguration: "
	       "IOPMConnectionAcknowledgeEvent failed, 0x%08x", ret);    
    }
    return;
}

static void
new_power_notification_init(void)
{
    IOPMConnection      connection = NULL;
    IOReturn            ret;
    
    ret = IOPMConnectionCreate(CFSTR("IPConfiguration"),
			       DISK_AND_NET,
			       &connection);
    if (ret != kIOReturnSuccess) {
	my_log(LOG_ERR,
	       "IPConfiguration: IOPMConnectionCreate failed, 0x%08x", ret);
	goto failed;
    }
    ret = IOPMConnectionSetNotification(connection, NULL,
					new_power_changed);
    
    if (ret != kIOReturnSuccess) {
	my_log(LOG_ERR, "IPConfiguration:"
	       "IOPMConnectionSetNotification failed, 0x%08x", ret);
	goto failed;
    }
    
    ret = IOPMConnectionScheduleWithRunLoop(connection, 
					    CFRunLoopGetCurrent(),
					    kCFRunLoopDefaultMode);
    if (ret != kIOReturnSuccess) {
	my_log(LOG_ERR, "IPConfiguration:"
	       "IOPMConnectionScheduleWithRunloop failed, 0x%08x", ret);
	goto failed;
    }
    return;

 failed:
    if (connection != NULL) {
	IOPMConnectionRelease(connection);
    }
    return;
}

#endif /* ! TARGET_OS_EMBEDDED */

static boolean_t
start_initialization(SCDynamicStoreRef session)
{
    CFPropertyListRef		value = NULL;

    S_observer = CFRunLoopObserverCreate(NULL, kCFRunLoopBeforeWaiting,
					 TRUE, 0, before_blocking, NULL);
    if (S_observer != NULL) {
	CFRunLoopAddObserver(CFRunLoopGetCurrent(), S_observer, 
			     kCFRunLoopDefaultMode);
    }
    else {
	my_log(LOG_NOTICE, 
	       "start_initialization: CFRunLoopObserverCreate failed!");
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
	my_log(LOG_NOTICE, 
	       "IPConfiguration needs PreferencesMonitor to run first");
    }
    my_CFRelease(&value);

    /* install run-time notifiers */
    notifier_init(session);

    (void)update_interface_list();

    (void)S_netboot_init();

    configure_from_cache(session);

    /* register for sleep/wake */
#if ! TARGET_OS_EMBEDDED
    if (S_use_maintenance_wake) {
	new_power_notification_init();
    }
    else {
	S_power_connection = power_notification_init();
    }
#else /* ! TARGET_OS_EMBEDDED */
    S_power_connection = power_notification_init();
#endif /* ! TARGET_OS_EMBEDDED */
    return (TRUE);
}

static void
link_refresh(SCDynamicStoreRef session, CFStringRef cache_key)
{
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
	/* don't propagate media status events for netboot interface */
	goto done;
    }
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	Service_t *	service_p = dynarray_element(&ifstate->services, j);

	config_method_renew(service_p);
    }

 done:
    my_CFRelease(&ifn_cf);
    return;
}

#include "my_darwin.h"

#ifndef NO_WIRELESS

#include <Apple80211/Apple80211API.h>
#include <Kernel/IOKit/apple80211/apple80211_ioctl.h>

static CFStringRef
S_copy_ssid(CFStringRef ifname)
{
    Apple80211Err	error;
    CFMutableDataRef	ssid;
    CFStringRef		ssid_str = NULL;
    Apple80211Ref	wref;
    
    error = Apple80211Open(&wref);
    if (error != kA11NoErr) {
	my_log(LOG_DEBUG, "Apple80211Open failed, 0x%x");
	return (NULL);
    }
    error = Apple80211BindToInterface(wref, ifname);
    if (error != kA11NoErr) {
	goto done;
    }
    ssid = CFDataCreateMutable(kCFAllocatorDefault, 0);
    if (Apple80211Get((Apple80211Ref)wref, APPLE80211_IOC_SSID, 0, 
		      ssid, 0) == kA11NoErr) {
	ssid_str = CFStringCreateWithBytes(NULL,
					   CFDataGetBytePtr(ssid),
					   CFDataGetLength(ssid),
					   kCFStringEncodingUTF8,
					   FALSE);
	if (ssid_str == NULL) {
	    ssid_str = CFStringCreateWithBytes(NULL,
					       CFDataGetBytePtr(ssid),
					       CFDataGetLength(ssid),
					       kCFStringEncodingMacRoman,
					       FALSE);
	}
    }
    CFRelease(ssid);

 done:
    Apple80211Close(wref);
    return (ssid_str);
}

#else NO_WIRELESS

static CFStringRef
S_copy_ssid(CFStringRef ifname)
{
    return (NULL);
}

#endif NO_WIRELESS

static void
link_key_changed(SCDynamicStoreRef session, CFStringRef cache_key)
{
    CFDictionaryRef		dict = NULL;
    interface_t *		if_p = NULL;
    CFStringRef			ifn_cf = NULL;
    char			ifn[IFNAMSIZ + 1];
    IFState_t *   		ifstate;
    int 			j;
    link_status_t		link;
    CFBooleanRef		link_val = NULL;
    boolean_t			network_changed = FALSE;

    ifn_cf = parse_component(cache_key, S_state_interface_prefix);
    if (ifn_cf == NULL) {
	return;
    }
    cfstring_to_cstring(ifn_cf, ifn, sizeof(ifn));
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, ifn, NULL);
    dict = my_SCDynamicStoreCopyValue(session, cache_key);
    if (dict != NULL) {
	if (CFDictionaryContainsKey(dict, kSCPropNetLinkDetaching)) {
	    if (ifstate != NULL) {
		IFState_services_free(ifstate);
	    }
	    goto done;
	}
	link_val = CFDictionaryGetValue(dict, kSCPropNetLinkActive);
	link_val = isA_CFBoolean(link_val);
    }
    if (link_val == NULL) {
	link.valid = link.active = FALSE;
    }
    else {
	link.valid = TRUE;
	link.active = CFEqual(link_val, kCFBooleanTrue);
    }
    if (link.valid) {
	if_p = ifl_find_name(S_interfaces, ifn);
	if (if_p != NULL) {
	    /* make sure address information is up to date */
	    if_link_update(if_p);
	}
	else {
	    if_p = NULL;
	}
    }
    if (ifstate == NULL || ifstate->netboot) {
	/* don't propagate media status events for netboot interface */
	goto done;
    }
    if (if_p != NULL) {
	if_link_copy(ifstate->if_p, if_p);
    }
    ifstate->link = link;
    if (link.valid == FALSE) {
	my_log(LOG_DEBUG, "%s link is unknown", ifn);
    }
    else {
	my_log(LOG_DEBUG, "%s link is %s",
	       ifn, link.active ? "up" : "down");
    }
    if (if_is_wireless(ifstate->if_p)) {
	CFStringRef	ssid;

	ssid = S_copy_ssid(ifstate->ifname);
	if (G_IPConfiguration_verbose) {
	    if (ssid != NULL) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("%s: SSID is %@"), 
		      if_name(ifstate->if_p), ssid);
	    }
	    else {
		my_log(LOG_NOTICE, "%s: no SSID",
		       if_name(ifstate->if_p));
	    }
	}
	if (ssid != NULL) {
	    if (ifstate->ssid != NULL
		&& !CFEqual(ssid, ifstate->ssid)) {
		network_changed = TRUE;
	    }
	    /* remember the last ssid */
	    IFState_set_ssid(ifstate, ssid);
	    CFRelease(ssid);
	}
    }
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	Service_t *	service_p = dynarray_element(&ifstate->services, j);

	config_method_media(service_p, network_changed);
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
    int		n_bytes = 0;
 
    arr = CFStringCreateArrayBySeparatingStrings(NULL, colon_hex, CFSTR(":"));
    if (arr != NULL) {
	n_bytes = CFArrayGetCount(arr);
    }
    if (n_bytes == 0) {
	goto failed;
    }
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
    cfstring_to_cstring(ifn_cf, ifn, sizeof(ifn));
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, ifn, NULL);
    if (ifstate == NULL || ifstate->netboot) {
	/* don't propogate collision events for netboot interface */
	goto done;
    }
    if (S_is_our_hardware_address(NULL, if_link_arptype(ifstate->if_p), 
				  hwaddr, hwlen)) {
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
    SCLog(G_IPConfiguration_verbose, LOG_NOTICE, 
	  CFSTR("Changes: %@ (%d)"), changes,
	  count);
    for (i = 0; i < count; i++) {
	CFStringRef	cache_key = CFArrayGetValueAtIndex(changes, i);
	if (CFEqual(cache_key, S_computer_name_key)
	    || CFEqual(cache_key, S_hostnames_key)) {
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
	else if (CFStringHasSuffix(cache_key, kSCEntNetRefreshConfiguration)) {
	    link_refresh(session, cache_key);
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
	S_linklocal_needs_attention = TRUE;
    }
 done:
    return;
}

#if ! TARGET_OS_EMBEDDED

static void
user_confirm(CFUserNotificationRef userNotification, 
	     CFOptionFlags response_flags)
{
    int 	i;

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

static CFURLRef
copy_icon_url(CFStringRef icon)
{
    CFBundleRef		np_bundle;
    CFURLRef		np_url;
    CFURLRef		url = NULL;

#define kNetworkPrefPanePath	"/System/Library/PreferencePanes/Network.prefPane"
    np_url = CFURLCreateWithFileSystemPath(NULL,
					   CFSTR(kNetworkPrefPanePath),
					   kCFURLPOSIXPathStyle, FALSE);
    if (np_url != NULL) {
	np_bundle = CFBundleCreate(NULL, np_url);
	if (np_bundle != NULL) {
	    url = CFBundleCopyResourceURL(np_bundle, icon, 
					  CFSTR("icns"), NULL);
	    CFRelease(np_bundle);
	}
	CFRelease(np_url);
    }
    return (url);
}

void
service_remove_conflict(Service_t * service_p)
{
    if (service_p->user_rls) {
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(), service_p->user_rls,
			      kCFRunLoopDefaultMode);
	my_CFRelease(&service_p->user_rls);
    }
    if (service_p->user_notification != NULL) {
	CFUserNotificationCancel(service_p->user_notification);
	my_CFRelease(&service_p->user_notification);
    }
    return;
}

static void
service_notify_user(Service_t * service_p, CFArrayRef header,
		    CFStringRef message)
{
    CFMutableDictionaryRef	dict;
    SInt32			error;
    CFURLRef			icon_url;
    CFUserNotificationRef 	notify;
    CFRunLoopSourceRef		rls;
    CFURLRef			url;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(dict, kCFUserNotificationAlertHeaderKey, 
			 header);
    CFDictionarySetValue(dict, kCFUserNotificationAlertMessageKey, 
			 message);
    url = CFBundleCopyBundleURL(S_bundle);
    CFDictionarySetValue(dict, kCFUserNotificationLocalizationURLKey,
			 url);
    CFRelease(url);
    icon_url = copy_icon_url(CFSTR("Network"));
    if (icon_url != NULL) {
	CFDictionarySetValue(dict, kCFUserNotificationIconURLKey,
			     icon_url);
	CFRelease(icon_url);
    }
    service_remove_conflict(service_p);
    CFDictionaryAddValue(dict,
			 kCFUserNotificationHelpAnchorKey,
			 CFSTR("mh27606"));
    CFDictionaryAddValue(dict,
			 kCFUserNotificationHelpBookKey,
			 CFSTR("com.apple.machelp"));
    CFDictionaryAddValue(dict, kCFUserNotificationDefaultButtonTitleKey, 
			 CFSTR("OK"));
    notify = CFUserNotificationCreate(NULL, 0, 0, &error, dict);
    CFRelease(dict);
    if (notify == NULL) {
	my_log(LOG_ERR, "CFUserNotificationCreate() failed, %d",
	       error);
	return;
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
    return;
}

void
service_report_conflict(Service_t * service_p, struct in_addr * ip,   
                        const void * hwaddr, struct in_addr * server)
{
    CFArrayRef		array = NULL;
    CFStringRef         ip_str = NULL;
    const void *	values[3];

    /*
     * CONFLICT_HEADER_BEFORE_IP, CONFLICT_HEADER_AFTER_IP
     *
     * Unfortunately, we can't just use a format string with %@ because
     * the entity that displays the CFUserNotification (CFUN) needs to be able
     * to localize the alert strings based on the logged in user's localization.
     * If we localize the string here with variable data (the IP address),
     * there's no way for the CFUN to localize the string.
     *
     * The ugly work-around is to break the string into localizable pieces,
     * in this case, the string before the IP address, and the string after
     * the IP address.
     * 
     * We pass the three pieces { BEFORE_IP, ip_str, AFTER_IP } as an array
     * to the CFUN.  It will only be able to localize BEFORE_IP and AFTER_IP.
     */
    ip_str = CFStringCreateWithFormat(NULL, NULL,
				      CFSTR(IP_FORMAT), IP_LIST(ip));
    values[0] = CFSTR("CONFLICT_HEADER_BEFORE_IP");
    values[1] = ip_str;
    values[2] = CFSTR("CONFLICT_HEADER_AFTER_IP");
    array = CFArrayCreate(NULL, (const void **)values, 3,
			  &kCFTypeArrayCallBacks);
    service_notify_user(service_p, array, CFSTR("CONFLICT_MESSAGE"));
    CFRelease(ip_str);
    CFRelease(array);
    return;
}

#endif /* TARGET_OS_EMBEDDED */

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
    SCLog(G_IPConfiguration_verbose, LOG_NOTICE, 
	  CFSTR("%@ = %s"), key, ret == TRUE ? "true" : "false");
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
    SCLog(G_IPConfiguration_verbose, LOG_NOTICE, CFSTR("%@ = %d"), key, ret);
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
    SCLog(G_IPConfiguration_verbose, LOG_NOTICE, 
	  CFSTR("%@ = %d.%06d"), key, ret.tv_sec,
	  ret.tv_usec);
    return (ret);
}

static uint8_t *
S_get_char_array(CFArrayRef arr, int * len)
{
    uint8_t *	buf = NULL;
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
	    buf[real_count++] = (uint8_t) val;
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

static uint8_t *
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
	SCLog(G_IPConfiguration_verbose, LOG_NOTICE, 
	      CFSTR("SCPreferencesCreate DHCPClient failed: %s"),
	      SCErrorString(SCError()));
	return (NULL);
    }
    data = SCPreferencesGetValue(session, kDHCPClientApplicationPref);
    if (isA_CFDictionary(data) == NULL) {
	goto done;
    }
    if (data) {
	SCLog(G_IPConfiguration_verbose, LOG_NOTICE, 
	      CFSTR("dictionary is %@"), data);
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

static CFArrayRef
S_copy_plist_array_and_range(CFDictionaryRef plist, CFStringRef key,
			     CFRange * range_p)
{
    CFArrayRef		val;

    val = isA_CFArray(CFDictionaryGetValue(plist, key));
    if (val != NULL) {
	CFRetain(val);
	if (G_IPConfiguration_verbose) {
	    SCLog(TRUE, LOG_NOTICE, CFSTR("%@ = %@"), key, val);
	}
	range_p->location = 0;
	range_p->length = CFArrayGetCount(val);
    }
    return (val);
}

static void
S_set_globals(void)
{
    CFURLRef 		dir_url;
    Boolean		got_path;
    CFPropertyListRef	plist;
    char		path[PATH_MAX];

    dir_url = CFBundleCopyResourcesDirectoryURL(S_bundle);
    got_path = CFURLGetFileSystemRepresentation(dir_url, TRUE, (UInt8 *)path,
						sizeof(path));
    CFRelease(dir_url);
    if (got_path == FALSE) {
	return;
    }
    strlcat(path, "/" IPCONFIGURATION_PLIST, sizeof(path));
    plist = my_CFPropertyListCreateFromFile(path);
    if (plist != NULL && isA_CFDictionary(plist) != NULL) {
	uint8_t *	dhcp_params = NULL;
	int		n_dhcp_params = 0;

	G_IPConfiguration_verbose 
	    = S_get_plist_boolean(plist, CFSTR("Verbose"), FALSE);
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
	S_arp_detect_count
	    = S_get_plist_int(plist, CFSTR("ARPDetectCount"), 
			      ARP_DETECT_COUNT);
	S_arp_detect_retry
	    = S_get_plist_timeval(plist, CFSTR("ARPDetectRetryTimeSeconds"), 
				  S_arp_detect_retry);
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
	G_dhcp_select_retry_count
	    = S_get_plist_int(plist, CFSTR("DHCPSelectRetryCount"), 
			      DHCP_SELECT_RETRY_COUNT);
	G_dhcp_allocate_linklocal_at_retry_count
	    = S_get_plist_int(plist, CFSTR("DHCPAllocateLinkLocalAtRetryCount"),
			      DHCP_ALLOCATE_LINKLOCAL_AT_RETRY_COUNT);
	G_dhcp_router_arp_at_retry_count
	    = S_get_plist_int(plist, CFSTR("DHCPRouterARPAtRetryCount"),
			      DHCP_ROUTER_ARP_AT_RETRY_COUNT);
	dhcp_params 
	    = S_get_plist_char_array(plist,
				     kDHCPRequestedParameterList,
				     &n_dhcp_params);
	dhcp_set_default_parameters(dhcp_params, n_dhcp_params);
	G_router_arp
	    = S_get_plist_boolean(plist, CFSTR("RouterARPEnabled"), TRUE);
	S_dhcp_local_hostname_length_max
	    = S_get_plist_int(plist, CFSTR("DHCPLocalHostNameLengthMax"),
			      DHCP_LOCAL_HOSTNAME_LENGTH_MAX);
	G_discover_and_publish_router_mac_address
	    = S_get_plist_boolean(plist,
				  CFSTR("DiscoverAndPublishRouterMACAddress"),
				  TRUE);
	S_discover_router_mac_address_secs
	    = S_get_plist_int(plist,
			      CFSTR("DiscoverRouterMACAddressTimeSeconds"),
			      DISCOVER_ROUTER_MAC_ADDRESS_SECS);
	G_dhcp_defend_ip_address_interval_secs
	    = S_get_plist_int(plist,
			      CFSTR("DHCPDefendIPAddressIntervalSeconds"),
			      DHCP_DEFEND_IP_ADDRESS_INTERVAL_SECS);
	G_dhcp_defend_ip_address_count
	    = S_get_plist_int(plist,
			      CFSTR("DHCPDefendIPAddressCount"),
			      DHCP_DEFEND_IP_ADDRESS_COUNT);
	G_dhcp_lease_write_t1_threshold_secs
	    = S_get_plist_int(plist, 
			      CFSTR("DHCPLeaseWriteT1ThresholdSeconds"),
			      DHCP_LEASE_WRITE_T1_THRESHOLD_SECS);
	S_router_arp_excluded_ssids
	    = S_copy_plist_array_and_range(plist,
					   CFSTR("RouterARPExcludedSSIDs"),
					   &S_router_arp_excluded_ssids_range);
	S_arp_conflict_retry
	    = S_get_plist_int(plist,
			      CFSTR("ARPConflictRetryCount"),
			      ARP_CONFLICT_RETRY_COUNT);
	S_arp_conflict_delay
	    = S_get_plist_timeval(plist, CFSTR("ARPConflictRetryDelaySeconds"), 
				  S_arp_conflict_delay);
	G_manual_conflict_retry_interval_secs
	    = S_get_plist_int(plist, 
			      CFSTR("ManualConflictRetryIntervalSeconds"),
			      MANUAL_CONFLICT_RETRY_INTERVAL_SECS);
#if ! TARGET_OS_EMBEDDED
	S_use_maintenance_wake
	    = S_get_plist_boolean(plist,
				  CFSTR("UseMaintenanceWake"),
				  TRUE);
#endif /* ! TARGET_OS_EMBEDDED */
    }
    my_CFRelease(&plist);
    return;
}

static void
S_add_dhcp_parameters()
{
    uint8_t *	dhcp_params = NULL;
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
    return;
}

void
start(const char *bundleName, const char *bundleDir)
{
    arp_session_values_t	arp_values;

    if (server_active()) {
	my_log(LOG_NOTICE, "ipconfig server already active");
	fprintf(stderr, "ipconfig server already active\n");
	return;
    }

    S_set_globals();
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

    S_readers = FDSet_init();
    if (S_readers == NULL) {
	my_log(LOG_DEBUG, "FDSet_init() failed");
	return;
    }
    G_bootp_session = bootp_session_init(S_readers, G_client_port);
    if (G_bootp_session == NULL) {
	my_log(LOG_DEBUG, "bootp_session_init() failed");
	return;
    }

    /* initialize the default values structure */
    bzero(&arp_values, sizeof(arp_values));
    arp_values.probe_count = &S_arp_probe_count;
    arp_values.probe_gratuitous_count = &S_arp_gratuitous_count;
    arp_values.probe_interval = &S_arp_retry;
    arp_values.detect_count = &S_arp_detect_count;
    arp_values.detect_interval = &S_arp_detect_retry;
    arp_values.conflict_retry_count = &S_arp_conflict_retry;
    arp_values.conflict_delay_interval = &S_arp_conflict_delay;
    G_arp_session = arp_session_init(S_readers, 
				     S_is_our_hardware_address,
				     &arp_values);
    if (G_arp_session == NULL) {
	my_log(LOG_DEBUG, "arp_session_init() failed");
	return;
    }

    if (G_IPConfiguration_verbose) {
	S_IPConfiguration_log_file = logfile_fopen(IPCONFIGURATION_BOOTP_LOG);
	bootp_session_set_debug(G_bootp_session, S_IPConfiguration_log_file);
    }
    dynarray_init(&S_ifstate_list, IFState_free, NULL);

    /* set the loopback interface address */
    set_loopback();
    return;
}

void
prime()
{
    if (G_bootp_session == NULL) {
	return;
    }
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

void
stop(CFRunLoopSourceRef	stopRls)
{
    if (G_bootp_session != NULL) {
	all_services_event(&S_ifstate_list, IFEventID_power_off_e);
    }
    CFRunLoopSourceSignal(stopRls);
}
