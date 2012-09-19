/*
 * Copyright (c) 1999-2012 Apple Inc. All rights reserved.
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
 * 
 * September 4, 2009	Dieter Siegmund (dieter@apple.com)
 * - added support for IPv6 configuration
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
#include <net/firewire.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <netinet/bootp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <paths.h>
#include <syslog.h>
#include <net/if_types.h>
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
#include <Availability.h>
#if ! TARGET_OS_EMBEDDED
#include <CoreFoundation/CFUserNotification.h>
#include <CoreFoundation/CFUserNotificationPriv.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#endif /* ! TARGET_OS_EMBEDDED */

#include "rfc_options.h"
#include "dhcp_options.h"
#include "interfaces.h"
#include "util.h"

#include "dhcplib.h"
#include "ioregpath.h"

#include "ipconfig_types.h"
#include "ipconfigd.h"
#include "server.h"
#include "timer.h"

#include "ipconfigd_globals.h"
#include "ipconfigd_threads.h"
#include "FDSet.h"

#include "symbol_scope.h"
#include "dprintf.h"
#include "cfutil.h"
#include "sysconfig.h"
#include "ifutil.h"
#include "rtutil.h"
#include "DHCPv6Client.h"
#include "DHCPv6Socket.h"
#include "IPConfigurationServiceInternal.h"

#define RIFLAGS_IADDR_VALID	((uint32_t)0x1)
#define RIFLAGS_HWADDR_VALID	((uint32_t)0x2)
#define RIFLAGS_ARP_VERIFIED	((uint32_t)0x4)
#define RIFLAGS_ALL_VALID	(RIFLAGS_IADDR_VALID | RIFLAGS_HWADDR_VALID | RIFLAGS_ARP_VERIFIED)

#define kARPResolvedIPAddress		CFSTR("ARPResolvedIPAddress")
#define kARPResolvedHardwareAddress	CFSTR("ARPResolvedHardwareAddress")

typedef struct {
    uint32_t			flags;
    struct in_addr		iaddr;
    uint8_t			hwaddr[MAX_LINK_ADDR_LEN];
} router_info_t;

typedef struct IFState * IFStateRef;

typedef struct {
    struct {
	struct in_addr		addr;
	struct in_addr		mask;
    } requested_ip;
    inet_addrinfo_t		info;
    router_info_t		router;
} ServiceIPv4, * ServiceIPv4Ref;

typedef struct {
    struct in6_addr		addr;
    int				prefix_length;
} inet6_addr_prefix_t;

typedef struct {
    inet6_addr_prefix_t		requested_ip;
} ServiceIPv6, * ServiceIPv6Ref;

struct ServiceInfo {
    CFStringRef			serviceID;
    IFStateRef			ifstate;
    ipconfig_method_t		method;
    ipconfig_status_t		status;
    boolean_t			is_dynamic;
    boolean_t			no_publish;
    boolean_t			ready;
    CFStringRef			parent_serviceID;
    CFStringRef			child_serviceID;
    dispatch_source_t		pid_source;
    void * 			private;
#if ! TARGET_OS_EMBEDDED
    CFUserNotificationRef	user_notification;
    CFRunLoopSourceRef		user_rls;
#endif /* ! TARGET_OS_EMBEDDED */
    union {
	ServiceIPv4		v4;
	ServiceIPv6		v6;
    } u;
};

struct IFState {
    interface_t *		if_p;
    CFStringRef			ifname;
    dynarray_t			services;
    dynarray_t			services_v6;
    ServiceRef			linklocal_service_p;
    boolean_t			startup_ready;
    boolean_t			netboot;
    CFStringRef			ssid;
    struct ether_addr		bssid;
    timer_callout_t *		timer;
};

typedef dynarray_t	IFStateList_t;

#define IS_IPV4		TRUE
#define IS_IPV6		FALSE

/* Bitfield to indicate if ssid/bssid has changed */
#define NETWORK_CHANGED_NONE ((uint32_t)0x0)
#define NETWORK_CHANGED_SSID ((uint32_t)0x1)
#define NETWORK_CHANGED_BSSID ((uint32_t)0x2)

#ifndef kSCEntNetRefreshConfiguration
#define kSCEntNetRefreshConfiguration	CFSTR("RefreshConfiguration")
#endif /* kSCEntNetRefreshConfiguration */

#ifndef kSCEntNetIPv4ARPCollision
#define kSCEntNetIPv4ARPCollision	CFSTR("IPv4ARPCollision")
#endif /* kSCEntNetIPv4ARPCollision */

#ifndef kSCEntNetDHCPv6
#define kSCEntNetDHCPv6			CFSTR("DHCPv6")
#endif /* kSCEntNetDHCPv6 */

#ifndef kSCValNetIPv4ConfigMethodFailover
static const CFStringRef kIPConfigurationConfigMethodFailover = CFSTR("Failover");
#define kSCValNetIPv4ConfigMethodFailover kIPConfigurationConfigMethodFailover
#endif /* kSCValNetIPv4ConfigMethodFailover */

#ifndef kSCValNetIPv6ConfigMethodLinkLocal
static const CFStringRef kIPConfigurationIPv6ConfigMethodLinkLocal = CFSTR("LinkLocal");
#define kSCValNetIPv6ConfigMethodLinkLocal kIPConfigurationIPv6ConfigMethodLinkLocal
#endif /* kSCValNetIPv6ConfigMethodLinkLocal */


#ifndef kSCPropNetIPv4FailoverAddressTimeout
static const CFStringRef kIPConfigurationFailoverAddressTimeout = CFSTR("FailoverAddressTimeout");
#define kSCPropNetIPv4FailoverAddressTimeout	kIPConfigurationFailoverAddressTimeout
#endif /* kSCPropNetIPv4FailoverAddressTimeout */

#ifndef kSCPropNetIgnoreLinkStatus
static const CFStringRef kIPConfigurationIgnoreLinkStatus = CFSTR("IgnoreLinkStatus");
#define kSCPropNetIgnoreLinkStatus	kIPConfigurationIgnoreLinkStatus
#endif /* kSCPropNetIgnoreLinkStatus */

#ifndef kSCPropNetIPv66to4Relay
static const CFStringRef kSCPropNetIPv66to4Relay = CFSTR("6to4Relay");
#endif /* kSCPropNetIPv66to4Relay */

#define kDHCPClientPreferencesID	CFSTR("DHCPClient.plist")
#define kDHCPClientApplicationPref	CFSTR("Application")
#define kDHCPRequestedParameterList	CFSTR("DHCPRequestedParameterList")

#define kDHCPv6RequestedOptions		CFSTR("DHCPv6RequestedOptions")

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

boolean_t			G_is_netboot;

/* 
 * Static: S_link_inactive_secs
 * Purpose:
 *   Time to wait after the link goes inactive before unpublishing 
 *   the interface state information
 */
static int			S_link_inactive_secs = LINK_INACTIVE_WAIT_SECS;

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

#define DHCPv6_ENABLED		TRUE
#define DHCPv6_STATEFUL_ENABLED	TRUE

boolean_t			G_dhcpv6_enabled = DHCPv6_ENABLED;
boolean_t			G_dhcpv6_stateful_enabled = DHCPv6_STATEFUL_ENABLED;
int				G_dhcp_duid_type = kDHCPDUIDTypeLLT;

const unsigned char		G_rfc_magic[4] = RFC_OPTIONS_MAGIC;
const struct in_addr		G_ip_broadcast = { INADDR_BROADCAST };
const struct in_addr		G_ip_zeroes = { 0 };

/* local variables */
static interface_list_t *	S_interfaces = NULL;
static CFBundleRef		S_bundle = NULL;
static CFRunLoopObserverRef	S_observer = NULL;
static boolean_t		S_linklocal_needs_attention = FALSE;
static IFStateList_t		S_ifstate_list;
static io_connect_t 		S_power_connection;
static SCDynamicStoreRef	S_scd_session = NULL;
static CFStringRef		S_setup_service_prefix = NULL;
static CFStringRef		S_state_interface_prefix = NULL;
static char * 			S_computer_name = NULL;
static CFStringRef		S_computer_name_key = NULL;
static CFStringRef		S_hostnames_key = NULL;
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
static struct timeval		S_arp_resolve_retry = {
    ARP_RESOLVE_RETRY_SECS,
    ARP_RESOLVE_RETRY_USECS
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

#define IPCONFIGURATION_BOOTP_LOG "/var/log/com.apple.IPConfiguration.bootp"
static FILE *	S_dhcp_packet_log;

#define IPCONFIGURATION_DHCPV6_LOG "/var/log/com.apple.IPConfiguration.DHCPv6"
static FILE *	S_dhcpv6_packet_log;

static struct in_addr		S_netboot_ip;
static struct in_addr		S_netboot_server_ip;
static char			S_netboot_ifname[IFNAMSIZ + 1];

#if ! TARGET_OS_EMBEDDED
static boolean_t		S_use_maintenance_wake = TRUE;
static boolean_t		S_awake = TRUE;
#endif /* ! TARGET_OS_EMBEDDED */

#ifdef __IPHONE_OS_VERSION_MIN_REQUIRED
#if __IPHONE_OS_VERSION_MIN_REQUIRED < 40000
#define IPV6_MAP_AUTOMATIC_TO_LINKLOCAL		TRUE
#endif /* __IPHONE_OS_VERSION_MIN_REQUIRED < 40000 */
#endif /* __IPHONE_OS_VERSION_MIN_REQUIRED */

#ifndef IPV6_MAP_AUTOMATIC_TO_LINKLOCAL
#define IPV6_MAP_AUTOMATIC_TO_LINKLOCAL		FALSE
#endif /* IPV6_MAP_AUTOMATIC_TO_LINKLOCAL */

static boolean_t		S_ipv6_map_automatic_to_linklocal = IPV6_MAP_AUTOMATIC_TO_LINKLOCAL;
static boolean_t		S_configure_ipv6 = TRUE;

#define PROP_SERVICEID		CFSTR("ServiceID")

/*
 * forward declarations
 */
static void
S_add_dhcp_parameters(SCPreferencesRef prefs);

static void
configuration_changed(SCDynamicStoreRef session);

static ipconfig_status_t
config_method_event(ServiceRef service_p, IFEventID_t event, void * data);

static ipconfig_status_t
config_method_start(ServiceRef service_p, ipconfig_method_t method,
		    ipconfig_method_data_t * method_data);


static ipconfig_status_t
config_method_change(ServiceRef service_p, ipconfig_method_t method,
		     ipconfig_method_data_t * method_data,
		     boolean_t * needs_stop);


static ipconfig_status_t
config_method_stop(ServiceRef service_p);

static ipconfig_status_t
config_method_media(ServiceRef service_p, void * network_changed);

static ipconfig_status_t
config_method_bssid_changed(ServiceRef service_p);

static ipconfig_status_t
config_method_renew(ServiceRef service_p);

static void
service_publish_clear(ServiceRef service_p);

static boolean_t
all_services_ready();

static void
S_linklocal_elect(CFArrayRef service_order);

static CFArrayRef
S_get_service_order(SCDynamicStoreRef session);

static __inline__ IFStateRef
service_ifstate(ServiceRef service_p)
{
    return (service_p->ifstate);
}

static boolean_t
S_get_plist_boolean_quiet(CFDictionaryRef plist, CFStringRef key,
			  boolean_t def);
static int
S_get_plist_int_quiet(CFDictionaryRef plist, CFStringRef key,
		       int def);

static unsigned int
S_get_service_rank(CFArrayRef arr, ServiceRef service_p);

static IFStateRef
IFStateList_ifstate_with_name(IFStateList_t * list, const char * ifname,
			      int * where);

static void
IFStateFreeService(IFStateRef ifstate, ServiceRef service_p);

static ServiceRef
IFState_service_with_ip(IFStateRef ifstate, struct in_addr iaddr);

static uint8_t 
IFState_set_ssid_bssid(IFStateRef ifstate, CFStringRef ssid, 
					   const struct ether_addr * bssid);

static void
S_linklocal_start(ServiceRef parent_service_p, boolean_t allocate);

static CFStringRef
S_copy_ssid_bssid(CFStringRef ifname, struct ether_addr * ap_mac);

static int
S_remove_ip_address(const char * ifname, struct in_addr this_ip);

STATIC ipconfig_status_t
S_remove_service_with_id_str(CFStringRef serviceID);

static ipconfig_status_t
method_info_from_dict(CFDictionaryRef dict,
		      ipconfig_method_t * ret_method,
		      ipconfig_method_data_t * * ret_method_data);
static ipconfig_status_t
method_info_from_ipv6_dict(CFDictionaryRef dict,
			   ipconfig_method_t * ret_method,
			   ipconfig_method_data_t * * ret_method_data);

STATIC CFDictionaryRef
ServiceIPv4CopyMergedDNS(ServiceRef service_p, dhcp_info_t * info_p);

STATIC CFDictionaryRef
ServiceIPv6CopyMergedDNS(ServiceRef service_p, dhcpv6_info_t * info_v6_p);

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
    ServiceRef				service_p;
    router_arp_status_t			status;

    service_p = (ServiceRef)arg1;
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
	bcopy(result->addr.target_hardware, service_p->u.v4.router.hwaddr, 
	      if_link_length(if_p));
	service_router_set_all_valid(service_p);
	my_log(LOG_DEBUG, "service_resolve_router_complete %s: ARP "
	       IP_FORMAT ": response received", if_name(if_p),
	       IP_LIST(&service_p->u.v4.router.iaddr));
	status = router_arp_status_success_e;
    }
    else {
	status = router_arp_status_no_response_e;
	my_log(LOG_DEBUG, "service_resolve_router_complete %s: ARP router " 
	       IP_FORMAT ": no response", if_name(if_p),
	       IP_LIST(&service_p->u.v4.router.iaddr));
    }
    (*callback_func)(service_p, status);
    return;
}

boolean_t
service_resolve_router(ServiceRef service_p, arp_client_t * arp,
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

boolean_t
service_populate_router_arpinfo(ServiceRef service_p, 
				arp_address_info_t * info_p)
{
    interface_t *       	if_p = service_interface(service_p);
    struct in_addr      	router_ip;
   
    if (G_discover_and_publish_router_mac_address == FALSE) {
	/* don't bother */
	return (FALSE);
    }

    service_router_clear_arp_verified(service_p);
    
    if (service_router_is_iaddr_valid(service_p) == 0) {
	my_log(LOG_DEBUG,
	       "service_populate_router_arpinfo %s: "
	       "Router IP address missing", 
	       if_name(if_p));
	return (FALSE);
    }

    router_ip = service_router_iaddr(service_p);

    my_log(LOG_DEBUG, "service_populate_router_arpinfo %s: " 
	   "found gateway " IP_FORMAT,
	   if_name(if_p), IP_LIST(&router_ip));


    info_p->target_ip = router_ip; 
    bcopy(service_router_hwaddr(service_p), info_p->target_hardware, 
	  service_router_hwaddr_size(service_p));
    
    return (TRUE);
}


boolean_t
service_update_router_address(ServiceRef service_p,
			      dhcpol_t * options, struct in_addr our_ip)
{
    struct in_addr *		router_p;

    router_p = dhcp_get_router_from_options(options, our_ip);
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
 ** Utility routines
 **/

#define STARTUP_KEY	CFSTR("Plugin:IPConfiguration")

static __inline__ void
unblock_startup(SCDynamicStoreRef session)
{
    (void)SCDynamicStoreSetValue(session, STARTUP_KEY, STARTUP_KEY);
}

/**
 ** Service, IFState routines
 **/
static void
ServiceFree(void * arg)
{
    IFStateRef		ifstate;
    ServiceRef		service_p = (ServiceRef)arg;

    if (G_IPConfiguration_verbose) {
	SCLog(TRUE, LOG_NOTICE, CFSTR("ServiceFree(%@) %s"), 
	      service_p->serviceID, ipconfig_method_string(service_p->method));
    }
    ifstate = service_ifstate(service_p);
    if (ifstate != NULL && ifstate->linklocal_service_p == service_p) {
	ifstate->linklocal_service_p = NULL;
    }
    config_method_stop(service_p);
    service_publish_clear(service_p);
#if ! TARGET_OS_EMBEDDED
    ServiceRemoveAddressConflict(service_p);
#endif /* ! TARGET_OS_EMBEDDED */
    my_CFRelease(&service_p->serviceID);
    my_CFRelease(&service_p->parent_serviceID);
    my_CFRelease(&service_p->child_serviceID);
    if (service_p->pid_source != NULL) {
	CFStringRef	serviceID;

	serviceID = (CFStringRef)dispatch_get_context(service_p->pid_source);
	CFRelease(serviceID);
	dispatch_source_cancel(service_p->pid_source);
	dispatch_release(service_p->pid_source);
	service_p->pid_source = NULL;
    }
    free(service_p);
    return;
}

static ServiceRef
ServiceCreate(IFStateRef ifstate, CFStringRef serviceID,
	      ipconfig_method_t method,
	      ipconfig_method_data_t * method_data,
	      ServiceRef parent_service_p, ipconfig_status_t * status_p)
{
    ServiceRef		service_p;
    ipconfig_status_t	status = ipconfig_status_success_e;

    if (method == ipconfig_method_linklocal_e
	&& ifstate->linklocal_service_p != NULL) {
	IFStateFreeService(ifstate,
			   ifstate->linklocal_service_p);
	/* side-effect: ifstate->linklocal_service_p = NULL */
    }
    service_p = (ServiceRef)malloc(sizeof(*service_p));
    if (service_p == NULL) {
	status = ipconfig_status_allocation_failed_e;
	goto failed;
    }
    bzero(service_p, sizeof(*service_p));
    service_p->method = method;
    service_p->ifstate = ifstate;
    if (serviceID != NULL) {
	service_p->serviceID = (void *)CFRetain(serviceID);
    }
    else {
	service_p->serviceID = (void *)
	    CFStringCreateWithFormat(NULL, NULL, 
				     CFSTR("%s-%s"),
				     ipconfig_method_string(method),
				     if_name(ifstate->if_p));
    }
    if (parent_service_p != NULL) {
	service_p->parent_serviceID 
	    = (void *)CFRetain(parent_service_p->serviceID);
    }
    status = config_method_start(service_p, method, method_data);
    if (status != ipconfig_status_success_e) {
	goto failed;
    }
    if (parent_service_p != NULL) {
	my_CFRelease(&parent_service_p->child_serviceID);
	parent_service_p->child_serviceID 
	    = (void *)CFRetain(service_p->serviceID);
    }

    /* keep track of which service is the linklocal service */
    if (service_p->method == ipconfig_method_linklocal_e) {
	ifstate->linklocal_service_p = service_p;
    }
    *status_p = status;
    return (service_p);

 failed:
    if (service_p != NULL) {
	my_CFRelease(&service_p->serviceID);
	my_CFRelease(&service_p->parent_serviceID);
	free(service_p);
    }
    *status_p = status;
    return (NULL);
}

static void
ServiceHandleProcessExit(dispatch_source_t source)
{
    pid_t		pid;
    CFStringRef		serviceID;
    ipconfig_status_t	status;

    pid = (pid_t)dispatch_source_get_handle(source);
    my_log(LOG_DEBUG, "IPConfiguration: pid %d exited", pid);
    serviceID = (CFStringRef)dispatch_get_context(source);
    status = S_remove_service_with_id_str(serviceID);
    if (status != ipconfig_status_success_e) {
	SCLog(TRUE, LOG_ERR,
	      CFSTR("IPConfiguration: failed to stop service %@, %s"),
	      serviceID, ipconfig_status_string(status));
    }
    return;
}

static CFRunLoopRef 	S_plugin_runloop;

static void
ServiceProcessExited(dispatch_source_t source)
{
    /* handle the source on our runloop to avoid locking issues */
    CFRunLoopPerformBlock(S_plugin_runloop,
			  kCFRunLoopDefaultMode,
			  ^{ ServiceHandleProcessExit(source); });
    CFRunLoopWakeUp(S_plugin_runloop);
    return;
}

static void
ServiceMonitorPID(ServiceRef service_p, pid_t pid)
{
    dispatch_source_t		source;

    if (S_plugin_runloop == NULL) {
	S_plugin_runloop = CFRunLoopGetCurrent();
    }
    source = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, pid,
				    DISPATCH_PROC_EXIT,
				    dispatch_get_main_queue());
    if (source == NULL) {
	my_log(LOG_ERR, "IPConfiguration: dispatch_source_create failed");
	return;
    }
    CFRetain(service_p->serviceID);
    dispatch_set_context(source, (void *)service_p->serviceID);
    dispatch_source_set_event_handler(source,
				      ^{ ServiceProcessExited(source); });
    dispatch_resume(source);
    service_p->pid_source = source;
    return;
}

static ServiceRef
IFStateGetServiceWithID(IFStateRef ifstate, CFStringRef serviceID, 
			boolean_t is_ipv4)
{
    int			i;
    dynarray_t *	list;

    if (is_ipv4) {
	list = &ifstate->services;
    }
    else {
	list = &ifstate->services_v6;
    }
    for (i = 0; i < dynarray_count(list); i++) {
	ServiceRef	service_p = dynarray_element(list, i);

	if (CFEqual(serviceID, service_p->serviceID)) {
	    return (service_p);
	}
    }
    return (NULL);
}


static ServiceRef
IFState_service_with_ip(IFStateRef ifstate, struct in_addr iaddr)
{
    int		j;

    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	ServiceRef	service_p = dynarray_element(&ifstate->services, j);

	if (service_p->u.v4.info.addr.s_addr == iaddr.s_addr) {
	    return (service_p);
	}
    }
    return (NULL);
}

/*
 * Function: IFStateGetServiceMatchingIPv4Method
 * Purpose:
 *   Find a service that "matches" the given requested IPv4 method.  A service
 *   "matches" if the method types are not manual (i.e. BOOTP, DHCP),
 *   or the method types are manual (Manual, Inform, Failover), and the
 *   requested IP address matches.
 * 
 *   If the parameter "just_dynamic" is TRUE, only match any dynamic
 *   service, otherwise match all services.
 *
 */
static ServiceRef
IFStateGetServiceMatchingIPv4Method(IFStateRef ifstate,
				     ipconfig_method_t method,
				     ipconfig_method_data_t * method_data,
				     boolean_t just_dynamic)
{
    int			i;
    boolean_t		is_dhcp_or_bootp = FALSE;
    boolean_t		is_manual;

    is_manual = ipconfig_method_is_manual(method);
    if (is_manual == FALSE) {
	is_dhcp_or_bootp = ipconfig_method_is_dhcp_or_bootp(method);
    }
    for (i = 0; i < dynarray_count(&ifstate->services); i++) {
	ServiceRef	service_p = dynarray_element(&ifstate->services, i);
	
	if (just_dynamic && service_p->is_dynamic == FALSE) {
	    continue;
	}
	if (is_manual) {
	    if (ipconfig_method_is_manual(service_p->method)
		&& (method_data->manual.addr.s_addr
		    == service_requested_ip_addr(service_p).s_addr)) {
		return (service_p);
	    }
	}
	else if (is_dhcp_or_bootp 
		 && ipconfig_method_is_dhcp_or_bootp(service_p->method)) {
	    return (service_p);
	}
	else if (service_p->method == method) {
	    return (service_p);
	}
    }
    return (NULL);
}

/*
 * Function: IFStateGetServiceMatchingIPv6Method
 * Purpose:
 *   Find a service that "matches" the given requested method.  A service
 *   "matches" if the method types are the same, and for manual method,
 *   the IPv6 addresses are the same.
 * 
 *   If the parameter "just_dynamic" is TRUE, only match any dynamic
 *   service, otherwise match all services.
 *
 */
static ServiceRef
IFStateGetServiceMatchingIPv6Method(IFStateRef ifstate,
				    ipconfig_method_t method,
				    ipconfig_method_data_t * method_data,
				    boolean_t just_dynamic)
{
    int			i;

    for (i = 0; i < dynarray_count(&ifstate->services_v6); i++) {
	ServiceRef	service_p = dynarray_element(&ifstate->services_v6, i);
	ServiceIPv6Ref	v6_p;
	
	if (just_dynamic && service_p->is_dynamic == FALSE) {
	    continue;
	}
	if (service_p->method != method) {
	    continue;
	}
	if (method != ipconfig_method_manual_v6_e) {
	    return (service_p);
	}
	v6_p = (ServiceIPv6Ref)service_p;
	if (IN6_ARE_ADDR_EQUAL(&method_data->manual_v6.addr,
			       &v6_p->requested_ip.addr)) {
	    return (service_p);
	}
    }
    return (NULL);
}

/*
 * Function: IFStateGetServiceMatchingMethod
 * Purpose:
 *   Find a service that "matches" the given requested method.
 */
static ServiceRef
IFStateGetServiceMatchingMethod(IFStateRef ifstate,
				ipconfig_method_t method,
				ipconfig_method_data_t * method_data,
				boolean_t just_dynamic)
{
    if (ipconfig_method_is_v4(method)) {
	return (IFStateGetServiceMatchingIPv4Method(ifstate, method, 
						     method_data,
						     just_dynamic));
    }
    return (IFStateGetServiceMatchingIPv6Method(ifstate, method, 
						method_data,
						just_dynamic));
}

static ServiceRef
IFStateGetServiceWithIPv4Method(IFStateRef ifstate,
				 ipconfig_method_t method,
				 ipconfig_method_data_t * method_data,
				 boolean_t just_dynamic)
{
    int			j;
    boolean_t		is_manual;

    is_manual = ipconfig_method_is_manual(method);
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	ServiceRef	service_p = dynarray_element(&ifstate->services, j);

	if (just_dynamic && service_p->is_dynamic == FALSE) {
	    continue;
	}
	if (method == service_p->method) {
	    if (is_manual == FALSE
		|| (method_data->manual.addr.s_addr
		    == service_requested_ip_addr(service_p).s_addr)) {
		return (service_p);
	    }
	}
    }
    return (NULL);
}
			    
static __inline__ ServiceRef
IFStateGetServiceWithIPv6Method(IFStateRef ifstate,
				ipconfig_method_t method,
				ipconfig_method_data_t * method_data,
				boolean_t just_dynamic)
{
    /* IFStateGetServiceMatchingIPv6Method is already an exact match */
    return (IFStateGetServiceMatchingIPv6Method(ifstate, method, 
						method_data,
						just_dynamic));
}

/*
 * Function: IFStateGetServiceWithMethod
 * Purpose:
 *   Find a service with the given method and method args.
 * 
 *   If the parameter "just_dynamic" is TRUE, only match any dynamic
 *   service, otherwise match all services.
 *
 */
static ServiceRef
IFStateGetServiceWithMethod(IFStateRef ifstate,
			    ipconfig_method_t method,
			    ipconfig_method_data_t * method_data,
			    boolean_t just_dynamic)
{
    if (ipconfig_method_is_v4(method)) {
	return (IFStateGetServiceWithIPv4Method(ifstate, method, 
						 method_data,
						 just_dynamic));
    }
    return (IFStateGetServiceWithIPv6Method(ifstate, method, 
					    method_data,
					    just_dynamic));
}

static void
S_FreeNonDynamicServices(dynarray_t * services_p)
{
    int	count;
    int	i;

    count = dynarray_count(services_p);
    for (i = 0; i < count; ) {
	ServiceRef	service_p = dynarray_element(services_p, i);

	if (service_p->is_dynamic) {
	    i++;
	    continue;
	}
	dynarray_free_element(services_p, i);
	count--;
    }
    return;
}

static void
IFStateFreeIPv4Services(IFStateRef ifstate, boolean_t all)
{
    if (all) {
	dynarray_free(&ifstate->services);
    }
    else {
	S_FreeNonDynamicServices(&ifstate->services);
    }
    ifstate->startup_ready = TRUE;
    if (dynarray_count(&ifstate->services) == 0
	&& if_ift_type(ifstate->if_p) != IFT_STF) {
	inet_detach_interface(if_name(ifstate->if_p));
    }
    return;
}

static void
IFStateFreeIPv6Services(IFStateRef ifstate, boolean_t all)
{
    if (all) {
	dynarray_free(&ifstate->services_v6);
    }
    else {
	S_FreeNonDynamicServices(&ifstate->services_v6);
    }
    if (dynarray_count(&ifstate->services_v6) == 0) {
	(void)inet6_linklocal_stop(if_name(ifstate->if_p));
	inet6_detach_interface(if_name(ifstate->if_p));
    }
    return;
}

static void
IFStateFreeServiceWithID(IFStateRef ifstate, CFStringRef serviceID, 
			 boolean_t is_ipv4)
{
    int			i;
    dynarray_t *	list;

    if (is_ipv4) {
	list = &ifstate->services;
    }
    else {
	list = &ifstate->services_v6;
    }
    for (i = 0; i < dynarray_count(list); i++) {
	ServiceRef	service_p = dynarray_element(list, i);

	if (CFEqual(serviceID, service_p->serviceID)) {
	    dynarray_free_element(list, i);
	    return;
	}
    }
    return;
}

static void
IFStateFreeService(IFStateRef ifstate, ServiceRef service_p)
{
    IFStateFreeServiceWithID(ifstate, service_p->serviceID,
			     ipconfig_method_is_v4(service_p->method));
    return;
}

static ipconfig_status_t
IFState_service_add(IFStateRef ifstate, CFStringRef serviceID, 
		    ipconfig_method_t method, void * method_data,
		    ServiceRef parent_service_p, ServiceRef * ret_service_p)
{
    interface_t *	if_p = ifstate->if_p;
    ServiceRef		service_p = NULL;
    ipconfig_status_t	status = ipconfig_status_success_e;
    boolean_t		started_v6_linklocal = FALSE;

    if (ipconfig_method_is_v4(method)) {
	/* attach IP */
	inet_attach_interface(if_name(if_p));
    }
    else {
	int	ift_type = if_ift_type(if_p);

	/* attach IPv6 */
	inet6_attach_interface(if_name(if_p));

	if (ift_type != IFT_LOOP && ift_type != IFT_STF) {
	    link_status_t	link = if_get_link_status(if_p);

	    /* start IPv6 Link Local */
	    if (link.valid == FALSE || link.active) {
		(void)inet6_linklocal_start(if_name(if_p));
		started_v6_linklocal = TRUE;
	    }
	}
    }
    /* try to configure the service */
    service_p = ServiceCreate(ifstate, serviceID, method, 
			      method_data,
			      parent_service_p, &status);
    if (service_p == NULL) {
	my_log(LOG_DEBUG, "status from %s was %s",
	       ipconfig_method_string(method), 
	       ipconfig_status_string(status));
	if (ipconfig_method_is_v4(method)) {
	    if (dynarray_count(&ifstate->services) == 0) {
		/* no services configured, detach IP again */
		ifstate->startup_ready = TRUE;
		inet_detach_interface(if_name(if_p));
	    }
	}
	else {
	    if (dynarray_count(&ifstate->services_v6) == 0) {
		if (started_v6_linklocal) {
		    (void)inet6_linklocal_stop(if_name(if_p));
		}
		inet6_detach_interface(if_name(if_p));
	    }
	}
	all_services_ready();
    }
    else if (ipconfig_method_is_v4(method)) {
	dynarray_add(&ifstate->services, service_p);
    }
    else {
	dynarray_add(&ifstate->services_v6, service_p);
    }

    if (ret_service_p) {
	*ret_service_p = service_p;
    }
    return (status);
}

static void
IFState_update_media_status(IFStateRef ifstate) 
{
    const char * 	ifname = if_name(ifstate->if_p);
    link_status_t	link;

    link = if_link_status_update(ifstate->if_p);
    if (link.valid == FALSE) {
	my_log(LOG_DEBUG, "%s link is unknown", ifname);
    }
    else {
	my_log(LOG_DEBUG, "%s link is %s", ifname, link.active ? "up" : "down");
    }
    if (if_is_wireless(ifstate->if_p)) {
	struct ether_addr	bssid;
	CFStringRef		ssid;

	ssid = S_copy_ssid_bssid(ifstate->ifname, &bssid);
	if (G_IPConfiguration_verbose) {
	    if (ssid != NULL) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("%s: SSID %@ BSSID %s"), 
		      if_name(ifstate->if_p), ssid, ether_ntoa(&bssid));
	    }
	    else {
		my_log(LOG_NOTICE, "%s: no SSID",
		       if_name(ifstate->if_p));
	    }
	}
	/* remember the ssid */
	IFState_set_ssid_bssid(ifstate, ssid, &bssid);
	my_CFRelease(&ssid);
    }
    return;
}

static IFStateRef
IFState_init(interface_t * if_p)
{
    IFStateRef ifstate;

    ifstate = malloc(sizeof(*ifstate));
    if (ifstate == NULL) {
	my_log(LOG_ERR, "IFState_init: malloc ifstate failed");
	return (NULL);
    }
    bzero(ifstate, sizeof(*ifstate));
    ifstate->if_p = if_dup(if_p);
    ifstate->ifname = CFStringCreateWithCString(NULL, if_name(if_p),
						kCFStringEncodingASCII);
    IFState_update_media_status(ifstate);
    ifstate->timer = timer_callout_init();
    dynarray_init(&ifstate->services, ServiceFree, NULL);
    dynarray_init(&ifstate->services_v6, ServiceFree, NULL);
    return (ifstate);
}

static uint8_t 
IFState_set_ssid_bssid(IFStateRef ifstate, CFStringRef ssid,
		       const struct ether_addr * bssid)
{
    uint8_t network_changed = NETWORK_CHANGED_NONE;
    
    if (ssid != NULL) {
	CFRetain(ssid);
    }
   
    if (my_CFEqual(ssid, ifstate->ssid) == FALSE) {
	network_changed |= NETWORK_CHANGED_SSID; 
    }
    
    if (ifstate->ssid != NULL) {
	CFRelease(ifstate->ssid);
    }

    ifstate->ssid = ssid;

    if (bssid != NULL 
	&& bcmp(bssid, &ifstate->bssid, sizeof(bssid)) != 0) {
        ifstate->bssid = *bssid;
	network_changed |= NETWORK_CHANGED_BSSID;
    }
    return (network_changed);
}

static void
IFState_free(void * arg)
{
    IFStateRef		ifstate = (IFStateRef)arg;

    if (G_IPConfiguration_verbose) {
	my_log(LOG_NOTICE, "IFState_free(%s)", if_name(ifstate->if_p));
    }
    IFStateFreeIPv4Services(ifstate, TRUE);
    IFStateFreeIPv6Services(ifstate, TRUE);
    my_CFRelease(&ifstate->ifname);
    IFState_set_ssid_bssid(ifstate, NULL, NULL);
    if_free(&ifstate->if_p);
    timer_callout_free(&ifstate->timer);
    free(ifstate);
    return;
}

static IFStateRef
IFStateList_ifstate_with_name(IFStateList_t * list, const char * ifname,
			      int * where)
{
    int i;

    for (i = 0; i < dynarray_count(list); i++) {
	IFStateRef	element = dynarray_element(list, i);
	if (strcmp(if_name(element->if_p), ifname) == 0) {
	    if (where != NULL) {
		*where = i;
	    }
	    return (element);
	}
    }
    return (NULL);
}

static IFStateRef
IFStateList_ifstate_create(IFStateList_t * list, interface_t * if_p)
{
    IFStateRef   	ifstate;

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
    IFStateRef	ifstate;
    int		where = -1;

    ifstate = IFStateList_ifstate_with_name(list, ifname, &where);
    if (ifstate == NULL) {
	return;
    }
    dynarray_free_element(list, where);
    return;
}

#ifdef DEBUG
static void
IFStateList_print(IFStateList_t * list)
{
    int i;
  
    printf("-------start--------\n");
    for (i = 0; i < dynarray_count(list); i++) {
	IFStateRef	ifstate = dynarray_element(list, i);
	int		j;

	printf("%s:", if_name(ifstate->if_p));
	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    ServiceRef	service_p = dynarray_element(&ifstate->services, j);

	    printf("%s%s", (j == 0) ? "" : ", ",
		   ipconfig_method_string(service_p->method));
	}
	printf("\n");
    }
    printf("-------end--------\n");
    return;
}
#endif /* DEBUG */

static IFStateRef
IFStateListGetServiceWithID(IFStateList_t * list, CFStringRef serviceID,
			    ServiceRef * ret_service, boolean_t is_ipv4)
{
    int 	i;

    for (i = 0; i < dynarray_count(list); i++) {
	IFStateRef	ifstate = dynarray_element(list, i);
	ServiceRef	service_p;

	service_p = IFStateGetServiceWithID(ifstate, serviceID, is_ipv4);
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

static IFStateRef
IFStateList_service_with_ip(IFStateList_t * list, struct in_addr iaddr,
			    ServiceRef * ret_service)
{
    int 	i;

    for (i = 0; i < dynarray_count(list); i++) {
	IFStateRef	ifstate = dynarray_element(list, i);
	ServiceRef	service_p;

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

boolean_t 
service_is_using_ip(ServiceRef exclude_service_p, struct in_addr iaddr)
{
    int         i;

    for (i = 0; i < dynarray_count(&S_ifstate_list); i++) {
	IFStateRef	ifstate = dynarray_element(&S_ifstate_list, i);
	int		j;

	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    ServiceRef service_p = dynarray_element(&ifstate->services, j);
	    
	    if (service_p == exclude_service_p) {
		continue;
	    }
	
	    if (service_p->u.v4.info.addr.s_addr == iaddr.s_addr) {
		return (FALSE);
            }
	}
    }
    return (TRUE);

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
#endif /* KERN_NETBOOT */

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
    IFStateRef		ifstate;
    boolean_t		is_dhcp = TRUE;
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
    /* ALIGN: CFDataGetBytePtr should be at least sizeof(uint64_t) */
    dhcp = (struct dhcp *)(void *)CFDataGetBytePtr(response);
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
    strlcpy(S_netboot_ifname, if_name(if_p), sizeof(S_netboot_ifname));
    G_is_netboot = TRUE;

 done:
    my_CFRelease(&chosen);
    return (G_is_netboot);
}

static void
set_entity_value(CFStringRef * entities, 
		 CFDictionaryRef * values, int size,
		 CFStringRef entity, CFDictionaryRef value,
		 int * count_p)
{
    int		i;

    i = *count_p;
    if (i >= size) {
	my_log(LOG_NOTICE, "IPConfiguration: set_entity_value %d >= %d",
	       i, size);
	return;
    }
    entities[i] = entity;
    values[i] = value;
    (*count_p)++;
    return;
}

const char *
ServiceGetMethodString(ServiceRef service_p)
{
    return (ipconfig_method_string(service_p->method));
}

static void
service_clear(ServiceRef service_p)
{
    service_p->ready = FALSE;
    service_p->status = ipconfig_status_success_e;
    return;
}

#define N_PUBLISH_ENTITIES	5

static void
service_publish_clear(ServiceRef service_p)
{
    CFDictionaryRef	dns_dict = NULL;
    CFStringRef		entities[N_PUBLISH_ENTITIES];
    int			entity_count;
    CFDictionaryRef	values[N_PUBLISH_ENTITIES];

    service_clear(service_p);
    if (S_scd_session == NULL) {
	return;
    }
    if (ServiceIsIPv4(service_p)) {
	/* IPv4 */
	entity_count = 0;
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kSCEntNetIPv4, NULL, &entity_count);
	dns_dict = ServiceIPv4CopyMergedDNS(service_p, NULL);
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kSCEntNetDNS, dns_dict, &entity_count);
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kSCEntNetDHCP, NULL, &entity_count);
#if ! TARGET_OS_EMBEDDED
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kSCEntNetSMB, NULL, &entity_count);
#endif /* ! TARGET_OS_EMBEDDED */
    }
    else {
	/* IPv6 */
	entity_count = 0;
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kSCEntNetIPv6, NULL, &entity_count);
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kSCEntNetDHCPv6, NULL, &entity_count);
	dns_dict = ServiceIPv6CopyMergedDNS(service_p, NULL);
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kSCEntNetDNS, dns_dict, &entity_count);
    }
    my_SCDynamicStoreSetService(S_scd_session,
				service_p->serviceID,
				entities, values, entity_count,
				service_p->no_publish);
    my_CFRelease(&dns_dict);
    return;
}

static boolean_t
all_services_ready()
{
    int 		i;

    for (i = 0; i < dynarray_count(&S_ifstate_list); i++) {
	int		j;
	IFStateRef	ifstate = dynarray_element(&S_ifstate_list, i);

	if (dynarray_count(&ifstate->services) == 0
	    && ifstate->startup_ready == FALSE) {
	    return (FALSE);
	}
	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    ServiceRef	service_p = dynarray_element(&ifstate->services, j);

	    if (service_p->ready == FALSE) {
		return (FALSE);
	    }
	}
    }
    unblock_startup(S_scd_session);
    return (TRUE);
}

static void
dict_insert_router_info(ServiceRef service_p, CFMutableDictionaryRef dict)
{
    interface_t *		if_p = service_interface(service_p);
    char			link_addr[MAX_LINK_ADDR_LEN * 3];
    CFStringRef			link_addr_cf;
    CFStringRef			router_ip;
    CFStringRef			sig_str;

    if (service_router_all_valid(service_p) == FALSE) {
	return;
    }

    /* router IP address */
    router_ip
	= CFStringCreateWithFormat(NULL, NULL, 
				   CFSTR(IP_FORMAT),
				   IP_LIST(&service_p->u.v4.router.iaddr));
    /* router link address */
    link_addr_to_string(link_addr, sizeof(link_addr),
			service_p->u.v4.router.hwaddr,
			if_link_length(if_p));
    link_addr_cf = CFStringCreateWithCString(NULL,
					     link_addr,
					     kCFStringEncodingASCII);

    /* signature */
    sig_str
	= CFStringCreateWithFormat(NULL, NULL, 
				   CFSTR("IPv4.Router=%@;IPv4.RouterHardwareAddress=%s"),
				   router_ip,
				   link_addr);
    CFDictionarySetValue(dict, kNetworkSignature, sig_str);
    CFDictionarySetValue(dict, kARPResolvedIPAddress, router_ip);
    CFDictionarySetValue(dict, kARPResolvedHardwareAddress, link_addr_cf);
    CFRelease(sig_str);
    CFRelease(router_ip);
    CFRelease(link_addr_cf);
    return;
}

STATIC CFDictionaryRef
ServiceIPv4CopyMergedDNS(ServiceRef service_p, dhcp_info_t * info_p)
{
    dhcpv6_info_t	info_v6;
    ServiceRef		ipv6_service_p;

    ipv6_service_p = IFStateGetServiceWithID(service_p->ifstate, 
					     service_p->serviceID, 
					     IS_IPV6);
    bzero(&info_v6, sizeof(info_v6));
    if (ipv6_service_p != NULL) {
	(void)config_method_event(ipv6_service_p, IFEventID_get_dhcpv6_info_e,
				  &info_v6);
    }
    return (DNSEntityCreateWithDHCPv4AndDHCPv6Info(info_p, &info_v6));
}

void
ServicePublishSuccessIPv4(ServiceRef service_p, dhcp_info_t * dhcp_info_p)
{
    CFDictionaryRef		dhcp_dict = NULL;
    CFDictionaryRef		dns_dict = NULL;
    CFStringRef			entities[N_PUBLISH_ENTITIES];
    int				entity_count;
    inet_addrinfo_t *		info_p;
    CFMutableDictionaryRef	ipv4_dict = NULL;
    dhcpol_t *			options = NULL;
    ServiceRef			parent_service_p = NULL;
#if ! TARGET_OS_EMBEDDED
    CFMutableDictionaryRef	smb_dict = NULL;
    const uint8_t *		smb_nodetype = NULL;
    int				smb_nodetype_len = 0;
    struct in_addr *		smb_server = NULL;
    int				smb_server_len = 0;
#endif /* ! TARGET_OS_EMBEDDED */
    CFDictionaryRef		values[N_PUBLISH_ENTITIES];

    if (service_p->serviceID == NULL) {
	return;
    }
    info_p = &service_p->u.v4.info;
    service_p->ready = TRUE;
    service_p->status = ipconfig_status_success_e;

    if (S_scd_session == NULL) {
	/* configd is not running */
	return;
    }
    if (dhcp_info_p != NULL) {
	options = dhcp_info_p->options;
    }
    if (service_p->parent_serviceID != NULL) {
	parent_service_p 
	    = IFStateGetServiceWithID(service_ifstate(service_p), 
				      service_p->parent_serviceID,
				      IS_IPV4);
	if (parent_service_p == NULL
	    || parent_service_p->u.v4.info.addr.s_addr != 0) {
	    return;
	}
    }

    ipv4_dict = CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
    /* set the ip address array */
    my_CFDictionarySetIPAddressAsArrayValue(ipv4_dict,
					    kSCPropNetIPv4Addresses,
					    info_p->addr);
    /* set the ip mask array */
    my_CFDictionarySetIPAddressAsArrayValue(ipv4_dict,
					    kSCPropNetIPv4SubnetMasks,
					    info_p->mask);

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

    /* IPv4 */
    if (options != NULL) {
	char *		host_name = NULL;
	int		host_name_len = 0;

	if (service_p->method == ipconfig_method_bootp_e
	    || dhcp_parameter_is_ok(dhcptag_host_name_e)) {
	    host_name = (char *)
		dhcpol_find(options, 
			    dhcptag_host_name_e,
			    &host_name_len, NULL);
	    /* set the hostname */
	    if (host_name && host_name_len > 0) {
		CFStringRef		str;
		str = CFStringCreateWithBytes(NULL, (UInt8 *)host_name,
					      host_name_len,
					      kCFStringEncodingUTF8, 
					      FALSE);
		if (str != NULL) {
		    CFDictionarySetValue(ipv4_dict, CFSTR("Hostname"), str);
		    CFRelease(str);
		}
	    }
	}
	if (dhcp_parameter_is_ok(dhcptag_router_e)) {
	    struct in_addr *		router = NULL;

	    router = (struct in_addr *)
		dhcpol_find_with_length(options,
					dhcptag_router_e,
					sizeof(*router));
	    /* set the router */
	    if (router != NULL) {
		CFStringRef		str;
		
		str = my_CFStringCreateWithIPAddress(*router);
		CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Router, str);
		CFRelease(str);
	    }
	}
    }
    /* insert the signature */
    dict_insert_router_info(service_p, ipv4_dict);

    /*
     * Entity values can be NULL or not NULL.  The values are accumulated in
     * the "values" array.
     */
    entity_count = 0;
    set_entity_value(entities, values, N_PUBLISH_ENTITIES, 
		     kSCEntNetIPv4, ipv4_dict, &entity_count);

    /* DNS */
    if (parent_service_p != NULL) {
	dns_dict = ServiceIPv4CopyMergedDNS(parent_service_p, dhcp_info_p);
    }
    else {
	dns_dict = ServiceIPv4CopyMergedDNS(service_p, dhcp_info_p);
    }
    set_entity_value(entities, values, N_PUBLISH_ENTITIES,
		     kSCEntNetDNS, dns_dict, &entity_count);

#if ! TARGET_OS_EMBEDDED
    /* SMB */
    if (options != NULL) {
	if (dhcp_parameter_is_ok(dhcptag_nb_over_tcpip_name_server_e)) {
	    smb_server = (struct in_addr *)
		dhcpol_find(options, 
			    dhcptag_nb_over_tcpip_name_server_e,
			    &smb_server_len, NULL);
	}
	if (dhcp_parameter_is_ok(dhcptag_nb_over_tcpip_node_type_e)) {
	    smb_nodetype = (uint8_t *)
		dhcpol_find(options, 
			    dhcptag_nb_over_tcpip_node_type_e,
			    &smb_nodetype_len, NULL);
	}
    }
    if ((smb_server && smb_server_len >= sizeof(struct in_addr))
	|| (smb_nodetype && smb_nodetype_len == sizeof(uint8_t))) {
	smb_dict 
	    = CFDictionaryCreateMutable(NULL, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
	if (smb_server && smb_server_len >= sizeof(struct in_addr)) {
	    CFMutableArrayRef	array = NULL;
	    int			i;

	    array = CFArrayCreateMutable(NULL, 
					 smb_server_len / sizeof(struct in_addr),
					 &kCFTypeArrayCallBacks);
	    for (i = 0; i < (smb_server_len / sizeof(struct in_addr)); i++) {
		CFStringRef		str;
		str = my_CFStringCreateWithIPAddress(smb_server[i]);
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
    }
    set_entity_value(entities, values, N_PUBLISH_ENTITIES,
		     kSCEntNetSMB, smb_dict, &entity_count);
#endif /* ! TARGET_OS_EMBEDDED */
    
    /* DHCP */
    if (dhcp_info_p != NULL && dhcp_info_p->pkt_size != 0) {
	dhcp_dict = DHCPInfoDictionaryCreate(service_p->method, 
					     dhcp_info_p->options,
					     dhcp_info_p->lease_start,
					     dhcp_info_p->lease_expiration);
    }
    set_entity_value(entities, values, N_PUBLISH_ENTITIES,
		     kSCEntNetDHCP, dhcp_dict, &entity_count);

    if (parent_service_p != NULL) {
	my_SCDynamicStoreSetService(S_scd_session,
				    service_p->parent_serviceID,
				    entities, values, entity_count,
				    service_p->no_publish);
    }
    else {
	my_SCDynamicStoreSetService(S_scd_session,
				    service_p->serviceID,
				    entities, values, entity_count,
				    service_p->no_publish);
    }
    my_CFRelease(&ipv4_dict);
    my_CFRelease(&dns_dict);
    my_CFRelease(&dhcp_dict);
#if ! TARGET_OS_EMBEDDED
    my_CFRelease(&smb_dict);
#endif /* ! TARGET_OS_EMBEDDED */
    all_services_ready();
    return;
}

static void
my_CFDictionarySetIPv6AddressAsString(CFMutableDictionaryRef dict,
				      CFStringRef prop,
				      struct in6_addr * ip6_addr)
{
    CFStringRef		str;

    str = my_CFStringCreateWithIPv6Address(ip6_addr);
    CFDictionarySetValue(dict, prop, str);
    CFRelease(str);
    return;
}

static void
dict_set_inet6_info(CFMutableDictionaryRef dict, 
		    inet6_addrinfo_t * addr, int addr_count)
{
    CFMutableArrayRef	address_list;
    int			i;
    CFMutableArrayRef	prefix_list;

    address_list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    prefix_list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (i = 0; i < addr_count; i++) {
	CFStringRef	str;
	CFNumberRef	num;
	int		val;

	str = my_CFStringCreateWithIPv6Address(&(addr[i].addr));
	CFArrayAppendValue(address_list, str);
	CFRelease(str);
	val = addr[i].prefix_length;
	num = CFNumberCreate(NULL, kCFNumberIntType, &val);
	CFArrayAppendValue(prefix_list, num);
	CFRelease(num);
    }
    CFDictionarySetValue(dict, kSCPropNetIPv6Addresses, address_list);
    CFRelease(address_list);
    CFDictionarySetValue(dict, kSCPropNetIPv6PrefixLength, prefix_list);
    CFRelease(prefix_list);
    return;
}

STATIC CFDictionaryRef
ServiceIPv6CopyMergedDNS(ServiceRef service_p, dhcpv6_info_t * info_v6_p)
{
    dhcp_info_t		info;
    ServiceRef		ipv4_service_p;

    ipv4_service_p = IFStateGetServiceWithID(service_p->ifstate, 
					     service_p->serviceID, 
					     IS_IPV4);
    bzero(&info, sizeof(info));
    if (ipv4_service_p != NULL) {
	(void)config_method_event(ipv4_service_p, IFEventID_get_dhcp_info_e,
				  &info);
    }
    return (DNSEntityCreateWithDHCPv4AndDHCPv6Info(&info, info_v6_p));
}

void
ServicePublishSuccessIPv6(ServiceRef service_p,
			  inet6_addrinfo_t * addresses, int addresses_count,
			  struct in6_addr * router, int router_count,
			  dhcpv6_info_t * dhcp_info_p,
			  CFStringRef signature)
{
    CFStringRef			entities[N_PUBLISH_ENTITIES];
    int				entity_count;
    CFDictionaryRef		dhcp_dict = NULL;
    CFDictionaryRef		dns_dict = NULL;
    CFMutableDictionaryRef	ipv6_dict = NULL;
    DHCPv6OptionListRef		options = NULL;
    CFDictionaryRef		values[N_PUBLISH_ENTITIES];

    if (service_p->serviceID == NULL) {
	return;
    }
    if (addresses == NULL || addresses_count == 0) {
	return;
    }
    service_p->ready = TRUE;
    service_p->status = ipconfig_status_success_e;

    if (S_scd_session == NULL) {
	/* configd is not running */
	return;
    }

    if (dhcp_info_p != NULL) {
	options = dhcp_info_p->options;
    }

    /* IPv6 */
    ipv6_dict = CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);

    /* Addresses, PrefixLength */
    dict_set_inet6_info(ipv6_dict, addresses, addresses_count);

    /* Router */
    if (router != NULL) {
	/* only deal with one Router value currently - XXX */
	my_CFDictionarySetIPv6AddressAsString(ipv6_dict,
					      kSCPropNetIPv6Router,
					      router);
    }
    /* InterfaceName */
    CFDictionarySetValue(ipv6_dict, CFSTR("InterfaceName"),
			 service_ifstate(service_p)->ifname);
    /* signature */
    if (signature != NULL) {
	CFDictionarySetValue(ipv6_dict, kNetworkSignature,
			     signature);
    }

    /* DNS */
    dns_dict = ServiceIPv6CopyMergedDNS(service_p, dhcp_info_p);

    /* DHCPv6 */
    if (options != NULL) {
	dhcp_dict = DHCPv6InfoDictionaryCreate(options);
    }

    /*
     * Entity values can be NULL or not NULL.  The values are accumulated in
     * the "values" array.
     */
    entity_count = 0;
    set_entity_value(entities, values, N_PUBLISH_ENTITIES, 
		     kSCEntNetIPv6, ipv6_dict, &entity_count);
    set_entity_value(entities, values, N_PUBLISH_ENTITIES, 
		     kSCEntNetDNS, dns_dict, &entity_count);
    set_entity_value(entities, values, N_PUBLISH_ENTITIES, 
		     kSCEntNetDHCPv6, dhcp_dict, &entity_count);
    my_SCDynamicStoreSetService(S_scd_session,
				service_p->serviceID,
				entities, values, entity_count,
				service_p->no_publish);
    my_CFRelease(&ipv6_dict);
    my_CFRelease(&dns_dict);
    my_CFRelease(&dhcp_dict);
    return;
}

void
service_publish_failure_sync(ServiceRef service_p, ipconfig_status_t status,
			     boolean_t sync)
{
    if (ipconfig_method_is_v4(service_p->method)) {
	ServiceRef	child_service_p = NULL;
	ServiceRef	parent_service_p = NULL;

	if (service_p->child_serviceID != NULL) {
	    child_service_p 
		= IFStateGetServiceWithID(service_ifstate(service_p), 
					  service_p->child_serviceID,
					  IS_IPV4);
	}
	if (service_p->parent_serviceID != NULL) {
	    parent_service_p 
		= IFStateGetServiceWithID(service_ifstate(service_p), 
					  service_p->parent_serviceID,
					  IS_IPV4);
	}
	if (child_service_p != NULL
	    && child_service_p->u.v4.info.addr.s_addr != 0) {
	    ServicePublishSuccessIPv4(child_service_p, NULL);
	    service_clear(service_p);
	}
	else if (parent_service_p != NULL
		 && parent_service_p->u.v4.info.addr.s_addr == 0) {
	    ipconfig_status_t status;
	    
	    /* clear the information in the DynamicStore, but not the status */
	    status = parent_service_p->status;
	    service_publish_clear(parent_service_p);
	    parent_service_p->status = status;
	}
	else {
	    service_publish_clear(service_p);
	}
    }
    else {
	service_publish_clear(service_p);
    }
    service_p->ready = TRUE;
    service_p->status = status;
    my_log(LOG_DEBUG, "%s %s: status = '%s'",
	   ServiceGetMethodString(service_p),
	   if_name(service_interface(service_p)), 
	   ipconfig_status_string(status));
    if (sync == TRUE) {
	all_services_ready();
    }
    return;
}

void
service_publish_failure(ServiceRef service_p, ipconfig_status_t status)
{
    service_publish_failure_sync(service_p, status, TRUE);
    return;
}

int
service_enable_autoaddr(ServiceRef service_p)
{
    return (inet_set_autoaddr(if_name(service_interface(service_p)), 1));
}

int
service_disable_autoaddr(ServiceRef service_p)
{
    flush_routes(if_link_index(service_interface(service_p)),
		 G_ip_zeroes, G_ip_zeroes);
    return (inet_set_autoaddr(if_name(service_interface(service_p)), 0));
}

#define RANK_LOWEST	(1024 * 1024)
#define RANK_NONE	(RANK_LOWEST + 1)

static unsigned int
S_get_service_rank(CFArrayRef arr, ServiceRef service_p)
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
    ipv4_dict = my_SCDynamicStoreCopyDictionary(session, ipv4_key);
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
ServiceRef
service_parent_service(ServiceRef service_p)
{
    ipconfig_method_t		method;

    if (service_p == NULL || service_p->parent_serviceID == NULL) {
	return (NULL);
    }
    method = service_p->method;
    return (IFStateGetServiceWithID(service_ifstate(service_p), 
				    service_p->parent_serviceID,
				    ipconfig_method_is_v4(method)));
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
service_should_do_router_arp(ServiceRef service_p)
{
    IFStateRef		ifstate = service_ifstate(service_p);
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
 *   either allocate or not allocate an IP.
 */
void
linklocal_service_change(ServiceRef parent_service_p, boolean_t allocate)
{
    ipconfig_method_data_t	method_data;
    IFStateRef			ifstate = service_ifstate(parent_service_p);
    ServiceRef			ll_service_p;
    ServiceRef			ll_parent_p = NULL;
    boolean_t			needs_stop;

    /* if the interface has a user-configured service, ignore this request */
    ll_service_p = ifstate->linklocal_service_p;
    if (ll_service_p != NULL) {
	if (ll_service_p->parent_serviceID == NULL) {
	    /* don't touch user-configured link-local service */
	    return;
	}
	ll_parent_p = IFStateGetServiceWithID(ifstate,
					      ll_service_p->parent_serviceID,
					      IS_IPV4);
    }
    if (ll_parent_p == NULL) {
	linklocal_set_needs_attention();
	return;
    }
    if (parent_service_p != ll_parent_p) {
	/* we're not the one that triggered the link-local service */
	linklocal_set_needs_attention();
	return;
    }
    bzero(&method_data, sizeof(method_data));
    method_data.linklocal.allocate = allocate;
    (void)config_method_change(ll_service_p,
			       ipconfig_method_linklocal_e,
			       &method_data, &needs_stop);
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
S_linklocal_start(ServiceRef parent_service_p, boolean_t allocate)

{
    ipconfig_method_data_t	method_data;
    IFStateRef			ifstate = service_ifstate(parent_service_p);
    ServiceRef			service_p;
    ipconfig_status_t		status;

    bzero(&method_data, sizeof(method_data));
    method_data.linklocal.allocate = allocate;
    status = IFState_service_add(ifstate, NULL, ipconfig_method_linklocal_e,
				 &method_data, parent_service_p,
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
    unsigned int	all_best_rank = RANK_NONE;
    ServiceRef		all_best_service_p = NULL;
    int 		i;
    struct in_addr	mask;
    struct in_addr	netaddr;

    for (i = 0; i < dynarray_count(&S_ifstate_list); i++) {
	unsigned int	best_rank = RANK_NONE;
	ServiceRef	best_service_p = NULL;
	boolean_t	election_required = TRUE;
	IFStateRef	ifstate = dynarray_element(&S_ifstate_list, i);
	int		j;
	ServiceRef 	ll_parent_p = NULL;
	ServiceRef	ll_service_p;
	unsigned int	rank;

	if (if_ift_type(ifstate->if_p) == IFT_LOOP) {
	    /* skip loopback interface */
	    continue;
	}
	ll_service_p = ifstate->linklocal_service_p;
	if (ll_service_p != NULL) {
	    if (ll_service_p->parent_serviceID == NULL) {
		election_required = FALSE;
		if (ll_service_p->u.v4.info.addr.s_addr != 0) {
		    best_service_p = ll_service_p;
		    best_rank = S_get_service_rank(service_order, ll_service_p);
		}
	    }
	    else {
		/* check whether linklocal parent service is still there */
		ll_parent_p
		    = IFStateGetServiceWithID(ifstate,
					      ll_service_p->parent_serviceID,
					      IS_IPV4);
		if (ll_parent_p == NULL) {
		    /* parent of link-local service is gone, child goes too */
		    IFStateFreeService(ifstate, ll_service_p);
		    ll_service_p = NULL;
		    /* side-effect: ifstate->linklocal_service_p = NULL */
		}
	    }
	}
	if (election_required) {
	    /* find the best parent service for the linklocal service */
	    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
		ServiceRef 			service_p;
		inet_addrinfo_t *		info_p;
		
		service_p = dynarray_element(&ifstate->services, j);
		if (service_p->method == ipconfig_method_linklocal_e) {
		    /* skip existing child linklocal service */
		    continue;
		}
		info_p = &service_p->u.v4.info;
		if (info_p->addr.s_addr == 0) {
		    if (service_p->method != ipconfig_method_dhcp_e
			|| G_dhcp_failure_configures_linklocal == FALSE
			|| (service_p->status != ipconfig_status_no_server_e)) {
			/* service isn't ready to be a parent */
			continue;
		    }
		}
		rank = S_get_service_rank(service_order, service_p);
		if (best_service_p == NULL
		    || rank < best_rank
		    || (best_service_p->u.v4.info.addr.s_addr == 0
			&& info_p->addr.s_addr != 0)) {
		    best_service_p = service_p;
		    best_rank = rank;
		}
	    }
	    if (ll_parent_p != best_service_p) {
		/* best parent service changed */
		if (ll_parent_p != NULL) {
		    my_CFRelease(&ll_parent_p->child_serviceID);
		    IFStateFreeService(ifstate, ll_service_p);
		}
		if (best_service_p != NULL) {
		    boolean_t	allocate = LINKLOCAL_NO_ALLOCATE;
		    
		    if (best_service_p->u.v4.info.addr.s_addr == 0) {
			/* service has no IP address, allocate a linklocal IP */
			allocate = LINKLOCAL_ALLOCATE;
		    }
		    S_linklocal_start(best_service_p, allocate);
		}
	    }
	}
	if (best_service_p != NULL) {
	    if (all_best_service_p == NULL || best_rank < all_best_rank) {
		all_best_service_p = best_service_p;
		all_best_rank = best_rank;
	    }
	}
    }

    /* remove or set the 169.254/16 subnet */
    mask.s_addr = htonl(IN_CLASSB_NET);
    netaddr.s_addr = htonl(IN_LINKLOCALNETNUM);
    if (all_best_service_p == NULL) {
	my_log(LOG_DEBUG, "removing 169.254/16 subnet");
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
		      if_name(service_interface(all_best_service_p))) != 0) {
	    const char * 	ifn;

	    ifn = if_name(service_interface(all_best_service_p));
	    subnet_route_delete(G_ip_zeroes, netaddr, mask);
	    subnet_route_add(all_best_service_p->u.v4.info.addr, 
			     netaddr, mask, ifn);
	    my_log(LOG_DEBUG,
		   "setting 169.254/16 subnet on %s", ifn);
		   
	}
	else {
	    my_log(LOG_DEBUG,
		   "leaving 169.254/16 subnet on %s",
		   if_name(route_if_p));
	}
    }
    return;
}

int
service_set_address(ServiceRef service_p, 
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

    if (G_IPConfiguration_verbose) {
	my_log(LOG_NOTICE, 
	       "%s %s: setting " IP_FORMAT " netmask " IP_FORMAT 
	       " broadcast " IP_FORMAT, 
	       ServiceGetMethodString(service_p),
	       if_name(if_p), 
	       IP_LIST(&addr), IP_LIST(&mask), IP_LIST(&broadcast));
    }
    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "service_set_address(%s): socket() failed, %s (%d)",
	       if_name(if_p), strerror(errno), errno);
    }
    else {
	inet_addrinfo_t *	info_p = &service_p->u.v4.info;

	if (inet_aifaddr(s, if_name(if_p), addr, &mask, &broadcast) < 0) {
	    ret = errno;
	    my_log(LOG_DEBUG, "service_set_address(%s) " 
		   IP_FORMAT " inet_aifaddr() failed, %s (%d)", if_name(if_p),
		   IP_LIST(&addr), strerror(errno), errno);
	}
	bzero(info_p, sizeof(*info_p));
	info_p->addr = addr;
	info_p->mask = mask;
	info_p->netaddr = netaddr;
	info_p->broadcast = broadcast;
	close(s);
	(void)host_route(RTM_DELETE, addr);
	(void)host_route(RTM_ADD, addr);
    }
    flush_routes(if_link_index(if_p), G_ip_zeroes, broadcast);
    linklocal_set_needs_attention();
    return (ret);
}

static int
S_remove_ip_address(const char * ifname, struct in_addr this_ip)
{
    int			ret = 0;
    int 		s;

    s = inet_dgram_socket();
    if (s < 0) {
	ret = errno;
	my_log(LOG_DEBUG, 
	       "S_remove_ip_address(%s) socket() failed, %s (%d)",
	       ifname, strerror(errno), errno);
    }	
    else { 
	if (inet_difaddr(s, ifname, this_ip) < 0) {
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
service_remove_address(ServiceRef service_p)
{
    interface_t *	if_p = service_interface(service_p);
    inet_addrinfo_t *	info_p = &service_p->u.v4.info;
    int			ret = 0;

    if (info_p->addr.s_addr != 0) {
	inet_addrinfo_t		saved_info;

	/* copy IP info then clear it so that it won't be elected */
	saved_info = service_p->u.v4.info;
	bzero(info_p, sizeof(*info_p));

	/* if no service on this interface refers to this IP, remove the IP */
	if (IFState_service_with_ip(service_ifstate(service_p),
				    saved_info.addr) == NULL) {
	    /*
	     * This can only happen if there's a manual/inform service 
	     * and a BOOTP/DHCP service with the same IP.  Duplicate
	     * manual/inform services are prevented when created.
	     */
	    if (G_IPConfiguration_verbose) {
		my_log(LOG_NOTICE, "%s %s: removing " IP_FORMAT, 
		       ServiceGetMethodString(service_p),
		       if_name(if_p), IP_LIST(&saved_info.addr));
	    }
	    ret = S_remove_ip_address(if_name(if_p), saved_info.addr);
	}
	/* if no service refers to this IP, remove the host route for the IP */
	if (IFStateList_service_with_ip(&S_ifstate_list, 
					saved_info.addr, NULL) == NULL) {
	    (void)host_route(RTM_DELETE, saved_info.addr);
	}
	flush_routes(if_link_index(if_p), 
		     saved_info.addr, saved_info.broadcast);
    }
    linklocal_set_needs_attention();
    return (ret);
}

/**
 ** ServiceRef accessor routines
 **/

PRIVATE_EXTERN interface_t *
service_interface(ServiceRef service_p)
{
    return (service_p->ifstate->if_p);
}

PRIVATE_EXTERN link_status_t
service_link_status(ServiceRef service_p)
{
    return (if_get_link_status(service_interface(service_p)));
}


PRIVATE_EXTERN bool
service_is_address_set(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	return (v4_p->info.addr.s_addr == v4_p->requested_ip.addr.s_addr);
    }
    return (FALSE);
}

PRIVATE_EXTERN void
service_set_requested_ip_addr(ServiceRef service_p, struct in_addr ip)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	v4_p->requested_ip.addr = ip;
    }
    return;
}

PRIVATE_EXTERN struct in_addr
service_requested_ip_addr(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	return (v4_p->requested_ip.addr);
    }
    return (G_ip_zeroes);
}

PRIVATE_EXTERN void
service_set_requested_ip_mask(ServiceRef service_p, struct in_addr mask)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	v4_p->requested_ip.mask = mask;
    }
    return;
}

PRIVATE_EXTERN struct in_addr
service_requested_ip_mask(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	return (v4_p->requested_ip.mask);
    }
    return (G_ip_zeroes);
}

PRIVATE_EXTERN boolean_t
service_router_is_hwaddr_valid(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	return ((v4_p->router.flags & RIFLAGS_HWADDR_VALID) != 0);
    }
    return (FALSE);
}

PRIVATE_EXTERN void
service_router_set_hwaddr_valid(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	v4_p->router.flags |= RIFLAGS_HWADDR_VALID;
    }
    return;
}

PRIVATE_EXTERN void
service_router_clear_hwaddr_valid(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	v4_p->router.flags &= ~RIFLAGS_HWADDR_VALID;
    }
    return;
}

PRIVATE_EXTERN boolean_t
service_router_is_iaddr_valid(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	return ((v4_p->router.flags & RIFLAGS_IADDR_VALID) != 0);
    }
    return (FALSE);
}

PRIVATE_EXTERN void
service_router_set_iaddr_valid(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	v4_p->router.flags |= RIFLAGS_IADDR_VALID;
    }
    return;
}

PRIVATE_EXTERN void
service_router_clear_iaddr_valid(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	v4_p->router.flags &= ~RIFLAGS_IADDR_VALID;
    }
    return;
}

PRIVATE_EXTERN boolean_t
service_router_is_arp_verified(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;
	return ((v4_p->router.flags & RIFLAGS_ARP_VERIFIED) != 0);
    }
    return (FALSE);
}

PRIVATE_EXTERN void
service_router_set_arp_verified(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	v4_p->router.flags |= RIFLAGS_ARP_VERIFIED;
    }
    return;
}

PRIVATE_EXTERN void
service_router_clear_arp_verified(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	v4_p->router.flags &= ~RIFLAGS_ARP_VERIFIED;
    }
    return;
}

PRIVATE_EXTERN void
service_router_clear(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	v4_p->router.flags = 0;
    }
    return;
}

PRIVATE_EXTERN uint8_t *
service_router_hwaddr(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;
	return (v4_p->router.hwaddr);
    }
    return (NULL);
}

PRIVATE_EXTERN int
service_router_hwaddr_size(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;
	return (sizeof(v4_p->router.hwaddr));
    }
    return (0);
}

PRIVATE_EXTERN struct in_addr
service_router_iaddr(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	return (v4_p->router.iaddr);
    }
    return (G_ip_zeroes);
}

PRIVATE_EXTERN void
service_router_set_iaddr(ServiceRef service_p, struct in_addr iaddr)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	v4_p->router.iaddr = iaddr;
    }
    return;
}

PRIVATE_EXTERN boolean_t
service_router_all_valid(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	return ((v4_p->router.flags & RIFLAGS_ALL_VALID) == RIFLAGS_ALL_VALID);
    }
    return (FALSE);
}

PRIVATE_EXTERN void
service_router_set_all_valid(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	v4_p->router.flags = RIFLAGS_ALL_VALID;
    }
    return;
}

PRIVATE_EXTERN boolean_t
ServiceIsIPv4(ServiceRef service_p)
{
    return (ipconfig_method_is_v4(service_p->method));
}

PRIVATE_EXTERN boolean_t
ServiceIsIPv6(ServiceRef service_p)
{
    return (ipconfig_method_is_v6(service_p->method));
}

PRIVATE_EXTERN boolean_t
ServiceIsNetBoot(ServiceRef service_p)
{
    return (service_p->ifstate->netboot);
}

PRIVATE_EXTERN void *
ServiceGetPrivate(ServiceRef service_p)
{
    return (service_p->private);
}

PRIVATE_EXTERN void
ServiceSetPrivate(ServiceRef service_p, void * private)
{
    service_p->private = private;
    return;
}

PRIVATE_EXTERN struct in_addr
ServiceGetActiveIPAddress(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	return (service_p->u.v4.info.addr);
    }
    return (G_ip_zeroes);
}

PRIVATE_EXTERN struct in_addr
ServiceGetActiveSubnetMask(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	return (service_p->u.v4.info.mask);
    }
    return (G_ip_zeroes);
}

PRIVATE_EXTERN void
ServiceSetStatus(ServiceRef service_p, ipconfig_status_t status)
{
    service_p->status = status;
    return;
}

PRIVATE_EXTERN void
ServiceSetRequestedIPv6Address(ServiceRef service_p,
			       const struct in6_addr * addr_p,
			       int prefix_length)
{
    if (ServiceIsIPv6(service_p) == FALSE) {
	return;
    }
    service_p->u.v6.requested_ip.addr = *addr_p;
    service_p->u.v6.requested_ip.prefix_length = prefix_length;
    return;
}

PRIVATE_EXTERN void
ServiceGetRequestedIPv6Address(ServiceRef service_p, 
			       struct in6_addr * addr_p,
			       int * prefix_length)
{
    if (ServiceIsIPv6(service_p) == FALSE) {
	return;
    }
    *addr_p = service_p->u.v6.requested_ip.addr; 
    *prefix_length = service_p->u.v6.requested_ip.prefix_length;
    return;
}

PRIVATE_EXTERN int
ServiceSetIPv6Address(ServiceRef service_p, const struct in6_addr * addr_p,
		      int prefix_length, u_int32_t valid_lifetime,
		      u_int32_t preferred_lifetime)
{
    interface_t *	if_p = service_interface(service_p);
    int			ret = 0;
    int			s;

    if (ServiceIsIPv6(service_p) == FALSE) {
	return (EINVAL);
    }
    if (G_IPConfiguration_verbose) {
	char 	ntopbuf[INET6_ADDRSTRLEN];

	my_log(LOG_NOTICE, "%s %s: setting %s/%d",
	       ServiceGetMethodString(service_p),
	       if_name(if_p),
	       inet_ntop(AF_INET6, addr_p, ntopbuf, sizeof(ntopbuf)),
	       prefix_length);
    }
    s = inet6_dgram_socket();
    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "ServiceSetIPv6Address(%s): socket() failed, %s (%d)",
	       if_name(if_p), strerror(errno), errno);
    }
    else {
	if (inet6_aifaddr(s, if_name(if_p), addr_p, NULL, prefix_length,
			  valid_lifetime, preferred_lifetime) < 0) {
	    ret = errno;
	    my_log(LOG_DEBUG,
		   "ServiceSetIPv6Address(%s): socket() failed, %s (%d)",
		   if_name(if_p), strerror(errno), errno);
	}
	close(s);
    }
    return (ret);
}

PRIVATE_EXTERN void
ServiceRemoveIPv6Address(ServiceRef service_p,
			 const struct in6_addr * addr_p, int prefix_length)
{
    interface_t *	if_p = service_interface(service_p);
    int			s;

    if (ServiceIsIPv6(service_p) == FALSE) {
	return;
    }
    if (IN6_IS_ADDR_UNSPECIFIED(addr_p)) {
	/* no address assigned */
	return;
    }
    if (G_IPConfiguration_verbose) {
	char 	ntopbuf[INET6_ADDRSTRLEN];

	my_log(LOG_NOTICE, 
	       "%s %s: removing %s/%d",
	       ServiceGetMethodString(service_p),
	       if_name(if_p),
	       inet_ntop(AF_INET6, addr_p, ntopbuf, sizeof(ntopbuf)),
	       prefix_length);
    }
    s = inet6_dgram_socket();
    if (s < 0) {
	my_log(LOG_ERR, 
	       "ServiceRemoveIPv6Address(%s): socket() failed, %s (%d)",
	       if_name(if_p), strerror(errno), errno);
    }
    else {
	inet6_difaddr(s, if_name(if_p), addr_p);
	close(s);
    }
    return;
}

/**
 ** other
 **/
static void
set_loopback()
{
    struct in_addr	loopback;
    struct in_addr	loopback_net;
    struct in_addr	loopback_mask;
    int 		s = inet_dgram_socket();

#ifndef INADDR_LOOPBACK_NET
#define	INADDR_LOOPBACK_NET		(u_int32_t)0x7f000000
#endif /* INADDR_LOOPBACK_NET */

    loopback.s_addr = htonl(INADDR_LOOPBACK);
    loopback_mask.s_addr = htonl(IN_CLASSA_NET);
    loopback_net.s_addr = htonl(INADDR_LOOPBACK_NET);

    if (s < 0) {
	my_log(LOG_ERR, 
	       "set_loopback(): socket() failed, %s (%d)",
	       strerror(errno), errno);
	return;
    }
    if (inet_aifaddr(s, "lo0", loopback, &loopback_mask, NULL) < 0) {
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

void
remove_unused_ip(const char * ifname, struct in_addr ip)
{
    IFStateRef 	ifstate;

    /* if no service on this interface refers to this IP, remove the IP */
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, ifname, NULL);
    if (ifstate != NULL
	&& IFState_service_with_ip(ifstate, ip) == NULL) {
	if (G_IPConfiguration_verbose) {
	    my_log(LOG_NOTICE, "IPConfiguration %s: removing " IP_FORMAT, 
		   ifname, IP_LIST(&ip));
	}
	S_remove_ip_address(if_name(ifstate->if_p), ip);
    }
    return;
}


/**
 ** Routines for MiG interface
 **/

extern ipconfig_status_t
ipconfig_method_info_from_plist(CFPropertyListRef plist,
				ipconfig_method_t * method_p,
				ipconfig_method_data_t * * method_data_p)
{
    CFDictionaryRef	dict;
    boolean_t		is_ipv4;
    ipconfig_status_t	status = ipconfig_status_invalid_parameter_e;

    *method_p = ipconfig_method_none_e;
    *method_data_p = NULL;

    if (plist == NULL) {
	/* NULL means no config method i.e. ipconfig_method_none_e */
	status = ipconfig_status_success_e;
	goto done;
    }
    if (isA_CFDictionary(plist) == NULL) {
	/* if specified, plist must be a dictionary */
	goto done;
    }
    /* if dictionary contains IPv4 dict, use that */
    dict = CFDictionaryGetValue((CFDictionaryRef)plist, kSCEntNetIPv4);
    if (dict != NULL) {
	is_ipv4 = TRUE;
    }
    else {
	dict = CFDictionaryGetValue((CFDictionaryRef)plist, kSCEntNetIPv6);
	if (dict != NULL) {
	    is_ipv4 = FALSE;
	}
	else {
	    dict = (CFDictionaryRef)plist;
	    is_ipv4 = TRUE;
	}
    }
    if (isA_CFDictionary(dict) == NULL) {
	my_log(LOG_NOTICE, "IPConfiguration: invalid IPv%c entity",
	       is_ipv4 ? '4' : '6');
	goto done;
    }
    if (CFDictionaryGetCount(dict) == 0) {
	*method_p = (is_ipv4)
	    ? ipconfig_method_none_v4_e
	    : ipconfig_method_none_v6_e;
	status = ipconfig_status_success_e;
	goto done;
    }
    if (is_ipv4) {
	status = method_info_from_dict(dict, method_p, method_data_p);
    }
    else {
	status = method_info_from_ipv6_dict(dict, method_p, method_data_p);
    }

 done:
    return (status);
}

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
service_get_option(ServiceRef service_p, int option_code, void * option_data,
		   unsigned int * option_dataCnt)
{
    boolean_t ret = FALSE;

    switch (service_p->method) {
      case ipconfig_method_inform_e:
      case ipconfig_method_dhcp_e:
      case ipconfig_method_bootp_e: {
	  void * 	data;
	  dhcp_info_t	dhcp_info;
	  int	 	len;

	  if (service_p->ready == FALSE) {
	      break;
	  }
	  bzero(&dhcp_info, sizeof(dhcp_info));
	  (void)config_method_event(service_p, IFEventID_get_dhcp_info_e,
				    &dhcp_info);
	  if (dhcp_info.pkt_size == 0) {
	      break; /* out of switch */
	  }
	  data = dhcpol_find(dhcp_info.options, option_code,
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
	if (S_dhcp_packet_log != NULL) {
	    (void)fclose(S_dhcp_packet_log);
	    S_dhcp_packet_log = NULL;
	}
	if (S_dhcpv6_packet_log != NULL) {
	    (void)fclose(S_dhcpv6_packet_log);
	    S_dhcpv6_packet_log = NULL;
	}
    }
    else {
	if (S_dhcp_packet_log == NULL) {
	    S_dhcp_packet_log 
		= logfile_fopen(IPCONFIGURATION_BOOTP_LOG);
	}
	if (G_dhcpv6_enabled && S_dhcpv6_packet_log == NULL) {
	    S_dhcpv6_packet_log
		= logfile_fopen(IPCONFIGURATION_DHCPV6_LOG);
	}
    }
    bootp_session_set_debug(G_bootp_session, S_dhcp_packet_log);
    DHCPv6SocketSetLogFile(S_dhcpv6_packet_log);

    return (ipconfig_status_success_e);
}

int
get_if_count()
{
    return (dynarray_count(&S_ifstate_list));
}

ipconfig_status_t
get_if_addr(const char * name, u_int32_t * addr)
{
    IFStateRef 	ifstate;
    int			j;

    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, name, NULL);
    if (ifstate == NULL) {
	return (ipconfig_status_interface_does_not_exist_e);
    }
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	ServiceRef service_p = dynarray_element(&ifstate->services, j);

	if (service_p->u.v4.info.addr.s_addr != 0) {
	    *addr = service_p->u.v4.info.addr.s_addr;
	    return (ipconfig_status_success_e);
	}
    }
    return (ipconfig_status_not_found_e);
}

ipconfig_status_t
get_if_option(const char * name, int option_code, void * option_data, 
	      unsigned int * option_dataCnt)
{
    int 		i;
    boolean_t		name_match;

    for (i = 0, name_match = FALSE;
	 i < dynarray_count(&S_ifstate_list) && name_match == FALSE;
	 i++) {
	IFStateRef 	ifstate = dynarray_element(&S_ifstate_list, i);
	int		j;

	if (name[0] != '\0') {
	    if (strcmp(if_name(ifstate->if_p), name) != 0) {
		continue;
	    }
	    name_match = TRUE;
	}
	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    ServiceRef service_p = dynarray_element(&ifstate->services, j);

	    if (service_get_option(service_p, option_code, option_data,
				   option_dataCnt)) {
		return (ipconfig_status_success_e);
	    }
	}
    }
    if (name_match == FALSE) {
	return (ipconfig_status_interface_does_not_exist_e);
    }
    return (ipconfig_status_not_found_e);
}

ipconfig_status_t
get_if_packet(const char * name, void * packet_data, 
	      unsigned int * packet_dataCnt)
{
    dhcp_info_t		dhcp_info;
    IFStateRef 		ifstate;
    int			j;

    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, name, NULL);
    if (ifstate == NULL) {
	return (ipconfig_status_interface_does_not_exist_e);
    }
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	ServiceRef service_p = dynarray_element(&ifstate->services, j);
	    
	switch (service_p->method) {
	case ipconfig_method_inform_e:
	case ipconfig_method_dhcp_e:
	case ipconfig_method_bootp_e:
	    if (service_p->ready == FALSE) {
		break;
	    }
	    bzero(&dhcp_info, sizeof(dhcp_info));
	    (void)config_method_event(service_p, IFEventID_get_dhcp_info_e,
				      &dhcp_info);
	    if (dhcp_info.pkt_size == 0
		|| dhcp_info.pkt_size > *packet_dataCnt) {
		break; /* out of switch */
	    }
	    *packet_dataCnt = dhcp_info.pkt_size;
	    bcopy(dhcp_info.pkt, packet_data, *packet_dataCnt);
	    return (ipconfig_status_success_e);
	default:
	    break;
	} /* switch */
    } /* for */
    return (ipconfig_status_not_found_e);
}

ipconfig_status_t
get_if_v6_packet(const char * name, void * packet_data, 
		 unsigned int * packet_dataCnt)
{
    dhcpv6_info_t	dhcp_info;
    IFStateRef 		ifstate;
    int			j;

    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, name, NULL);
    if (ifstate == NULL) {
	return (ipconfig_status_interface_does_not_exist_e);
    }
    for (j = 0; j < dynarray_count(&ifstate->services_v6); j++) {
	ServiceRef service_p = dynarray_element(&ifstate->services_v6, j);
	    
	switch (service_p->method) {
	case ipconfig_method_automatic_v6_e:
	case ipconfig_method_rtadv_e:
	    if (service_p->ready == FALSE) {
		break;
	    }
	    bzero(&dhcp_info, sizeof(dhcp_info));
	    (void)config_method_event(service_p, IFEventID_get_dhcpv6_info_e,
				      &dhcp_info);
	    if (dhcp_info.pkt_len == 0
		|| dhcp_info.pkt_len > *packet_dataCnt) {
		break; /* out of switch */
	    }
	    *packet_dataCnt = dhcp_info.pkt_len;
	    bcopy(dhcp_info.pkt, packet_data, *packet_dataCnt);
	    return (ipconfig_status_success_e);
	default:
	    break;
	} /* switch */
    } /* for */
    return (ipconfig_status_not_found_e);
}

static IPConfigFuncRef
lookup_func(ipconfig_method_t method)
{
    IPConfigFuncRef	func = NULL;

    switch (method) {
    case ipconfig_method_linklocal_e:
	func =  linklocal_thread;
	break;
    case ipconfig_method_inform_e:
	func =  inform_thread;
	break;
    case ipconfig_method_manual_e:
	func =  manual_thread;
	break;
    case ipconfig_method_dhcp_e:
	func =  dhcp_thread;
	break;
    case ipconfig_method_bootp_e:
	func =  bootp_thread;
	break;
    case ipconfig_method_failover_e:
	func =  failover_thread;
	break;
    case ipconfig_method_rtadv_e:
    case ipconfig_method_automatic_v6_e:
	if (S_configure_ipv6) {
	    if (S_ipv6_map_automatic_to_linklocal
		&& method == ipconfig_method_automatic_v6_e) {
		func = linklocal_v6_thread;
	    }
	    else {
		func = rtadv_thread;
	    }
	}
	break;
    case ipconfig_method_stf_e:
	if (S_configure_ipv6) {
	    func = stf_thread;
	}
	break;
    case ipconfig_method_manual_v6_e:
	if (S_configure_ipv6) {
	    func = manual_v6_thread;
	}
	break;
    case ipconfig_method_linklocal_v6_e:
	if (S_configure_ipv6) {
	    func = linklocal_v6_thread;
	}
	break;
    default:
	break;
    }
    return (func);
}

static ipconfig_status_t
config_method_start(ServiceRef service_p, ipconfig_method_t method,
		    ipconfig_method_data_t * method_data)
{
    IPConfigFuncRef		func;
    interface_t * 		if_p = service_interface(service_p);
    int				type = if_link_type(if_p);

    if (method == ipconfig_method_stf_e && type != IFT_STF) {
	/* can't do 6to4 over anything but IFT_STF */
	return (ipconfig_status_invalid_operation_e);
    }
    switch (type) {
    case IFT_STF:
	if (method != ipconfig_method_stf_e) {
	    /* stf interface only does 6to4 */
	    return (ipconfig_status_invalid_operation_e);
	}
	break;
    case IFT_IEEE1394:
	if (method == ipconfig_method_bootp_e) {
	    /* can't do BOOTP over firewire */
	    return (ipconfig_status_invalid_operation_e);
	}
	break;
    case IFT_ETHER:
	break;
    case IFT_LOOP:
	    if (method != ipconfig_method_manual_e
		&& method != ipconfig_method_manual_v6_e) {
		/* loopback interface only does MANUAL */
		return (ipconfig_status_invalid_operation_e);
	    }
	    break;
    default:
	switch (method) {
	case ipconfig_method_linklocal_e:
	case ipconfig_method_inform_e:
	case ipconfig_method_dhcp_e:
	case ipconfig_method_bootp_e:
	    /* can't do ARP over anything but Ethernet and FireWire */
	    return (ipconfig_status_invalid_operation_e);
	default:
	    break;
	}
    }
    func = lookup_func(method);
    if (func == NULL) {
	return (ipconfig_status_operation_not_supported_e);
    }
    return (*func)(service_p, IFEventID_start_e, method_data);
}

static ipconfig_status_t
config_method_change(ServiceRef service_p,
		     ipconfig_method_t method,
		     ipconfig_method_data_t * method_data,
		     boolean_t * needs_stop)
{
    change_event_data_t		change_event;
    IPConfigFuncRef		func;
    ipconfig_status_t		status;

    *needs_stop = FALSE;
    func = lookup_func(method);
    if (func == NULL) {
	return (ipconfig_status_operation_not_supported_e);
    }
    change_event.method_data = method_data;
    change_event.needs_stop = FALSE;
    status = (*func)(service_p, IFEventID_change_e, &change_event);
    *needs_stop = change_event.needs_stop;
    return (status);
}

static ipconfig_status_t
config_method_event(ServiceRef service_p, IFEventID_t event, void * data)
{
    ipconfig_status_t	status = ipconfig_status_success_e;
    IPConfigFuncRef	func;
    ipconfig_method_t	method = service_p->method;

    func = lookup_func(method);
    if (func == NULL) {
	my_log(LOG_NOTICE, 
	       "config_method_event(%s): lookup_func(%d) failed",
	       IFEventID_names(event), method);
	status = ipconfig_status_internal_error_e;
	goto done;
    }
    (*func)(service_p, event, data);

 done:
    return (status);
    
}

static ipconfig_status_t
config_method_stop(ServiceRef service_p)
{
    return (config_method_event(service_p, IFEventID_stop_e, NULL));
}

static ipconfig_status_t
config_method_media(ServiceRef service_p, void * network_changed)
{
    /* if there's a media event, we need to re-ARP */
    service_router_clear_arp_verified(service_p);
    return (config_method_event(service_p, IFEventID_link_status_changed_e, 
				network_changed));
}

static ipconfig_status_t
config_method_bssid_changed(ServiceRef service_p)
{
   /* if there is a bssid change, we need to re-ARP */
   service_router_clear_arp_verified(service_p);
   return (config_method_event(service_p, IFEventID_bssid_changed_e, 
			       NULL));

}

static ipconfig_status_t
config_method_renew(ServiceRef service_p)
{
    /* renew forces a re-ARP too */
    service_router_clear_arp_verified(service_p);
    return (config_method_event(service_p, IFEventID_renew_e, NULL));
}

static void
service_list_event(dynarray_t * services_p, IFEventID_t event, void * data)
{
    int		i;

    for (i = 0; i < dynarray_count(services_p); i++) {
	ServiceRef	service_p = dynarray_element(services_p, i);

	config_method_event(service_p, event, data);
    }
}

static void
IFStateList_all_services_event(IFStateList_t * list, 
			       IFEventID_t event, void * evdata)
{
    int 		i;
    int			if_count = dynarray_count(list);

    for (i = 0; i < if_count; i++) {
	IFStateRef		ifstate = dynarray_element(list, i);

	service_list_event(&ifstate->services, event, evdata);
	service_list_event(&ifstate->services_v6, event, evdata);
    }
    return;
}

ipconfig_status_t
set_if(const char * name, ipconfig_method_t method,
       ipconfig_method_data_t * method_data)
{
    interface_t * 	if_p = ifl_find_name(S_interfaces, name);
    IFStateRef   	ifstate;

    if (G_IPConfiguration_verbose) {
	my_log(LOG_NOTICE, "set %s %s", name, ipconfig_method_string(method));
    }
    if (if_p == NULL) {
	return (ipconfig_status_interface_does_not_exist_e);
    }
    ifstate = IFStateList_ifstate_create(&S_ifstate_list, if_p);
    if (ifstate == NULL) {
	return (ipconfig_status_allocation_failed_e);
    }
    /* stop existing services */
    if (method == ipconfig_method_none_e
	|| method == ipconfig_method_none_v4_e
	|| ipconfig_method_is_v4(method)) {
	IFStateFreeIPv4Services(ifstate, TRUE);
    }
    else {
	IFStateFreeIPv6Services(ifstate, TRUE);
    }
    switch (method) {
    case ipconfig_method_none_e:
    case ipconfig_method_none_v4_e:
    case ipconfig_method_none_v6_e:
	return (ipconfig_status_success_e);
    default:
	break;
    }

    /* add a new service */
    return (IFState_service_add(ifstate, NULL, method, method_data,
				NULL, NULL));
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
		   void * method_data, 
		   void * service_id, unsigned int * service_id_len,
		   CFDictionaryRef plist, pid_t pid)
{
    interface_t * 	if_p = ifl_find_name(S_interfaces, name);
    IFStateRef   	ifstate;
    unsigned int	in_length;
    ServiceRef		service_p;
    CFStringRef		serviceID;
    ipconfig_status_t	status;

    in_length = *service_id_len;
    *service_id_len = 0;
    switch (method) {
    case ipconfig_method_none_e:
    case ipconfig_method_none_v4_e:
    case ipconfig_method_none_v6_e:
	return (ipconfig_status_invalid_parameter_e);
    default:
	break;
    }
    if (if_p == NULL) {
	return (ipconfig_status_interface_does_not_exist_e);
    }
    ifstate = IFStateList_ifstate_create(&S_ifstate_list, if_p);
    if (ifstate == NULL) {
	return (ipconfig_status_allocation_failed_e);
    }
    service_p = IFStateGetServiceMatchingMethod(ifstate, method,
						method_data,
						FALSE);
    if (service_p != NULL) {
	boolean_t	needs_stop = FALSE;

	if (add_only) {
	    return (ipconfig_status_duplicate_service_e);
	}
	status = config_method_change(service_p, method,
				      method_data,
				      &needs_stop);
	if (status == ipconfig_status_success_e
	    && needs_stop == FALSE) {
	    return (ipconfig_status_success_e);
	}
	IFStateFreeService(ifstate, service_p);
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
				 NULL, &service_p);
    if (status == ipconfig_status_success_e) {
	boolean_t	no_publish = FALSE;
	CFDictionaryRef	options_dict = NULL;
	pid_t		monitor_pid = -1;
	CFIndex		len = 0;

	if (plist != NULL) {
	    options_dict = CFDictionaryGetValue(plist, 
						kIPConfigurationServiceOptions);
	    if (isA_CFDictionary(options_dict) != NULL) {
		if (S_get_plist_boolean_quiet(options_dict, 
					      kIPConfigurationServiceOptionMonitorPID,
					      FALSE)) {
		    monitor_pid = pid;
		}
		no_publish 
		    = S_get_plist_boolean_quiet(options_dict,
						kIPConfigurationServiceOptionNoPublish,
						FALSE);
	    }
	}
	service_p->is_dynamic = TRUE;
	service_p->no_publish = no_publish;
	if (monitor_pid != -1) {
	    ServiceMonitorPID(service_p, monitor_pid);
	}
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

PRIVATE_EXTERN ipconfig_status_t
add_service(const char * name, ipconfig_method_t method,
	    ipconfig_method_data_t * method_data,
	    void * service_id, unsigned int * service_id_len,
	    CFDictionaryRef plist, pid_t pid)
{
    return (add_or_set_service(name, method, TRUE, method_data,
			       service_id, service_id_len, plist, pid));
}

PRIVATE_EXTERN ipconfig_status_t
set_service(const char * name, ipconfig_method_t method,
	    ipconfig_method_data_t * method_data,
	    void * service_id, unsigned int * service_id_len)
{
    return (add_or_set_service(name, method, FALSE, method_data,
			       service_id, service_id_len, NULL, -1));
}

STATIC ipconfig_status_t
S_remove_service(IFStateRef ifstate, ServiceRef service_p)
{
    boolean_t		is_ipv4;

    is_ipv4 = ipconfig_method_is_v4(service_p->method);
    if (service_p->is_dynamic == FALSE) {
	return (ipconfig_status_invalid_operation_e);
    }
    if (G_IPConfiguration_verbose) {
	my_log(LOG_NOTICE, "remove_service %s %s", if_name(ifstate->if_p),
	       ServiceGetMethodString(service_p));
    }

    /* remove the service */
    IFStateFreeService(ifstate, service_p);
    if (is_ipv4 == FALSE
	&& dynarray_count(&ifstate->services_v6) == 0) {
	(void)inet6_linklocal_stop(if_name(ifstate->if_p));
	inet6_detach_interface(if_name(ifstate->if_p));
    }
    return (ipconfig_status_success_e);
}

STATIC ipconfig_status_t
S_remove_service_with_id_str(CFStringRef serviceID)
{
    IFStateRef   	ifstate;
    ServiceRef		service_p;
    ipconfig_status_t	status;

    ifstate = IFStateListGetServiceWithID(&S_ifstate_list,
					  serviceID, &service_p,
					  IS_IPV6);
    if (ifstate == NULL) {
	ifstate = IFStateListGetServiceWithID(&S_ifstate_list,
					      serviceID, &service_p,
					      IS_IPV4);
    }
    if (ifstate == NULL) {
	status = ipconfig_status_no_such_service_e;
    }
    else {
	status = S_remove_service(ifstate, service_p);
    }
    return (status);
}

PRIVATE_EXTERN ipconfig_status_t
remove_service_with_id(void * service_id, unsigned int service_id_len)
{
    CFStringRef		serviceID;
    ipconfig_status_t	status;

    serviceID = CFStringCreateWithBytes(NULL, service_id, service_id_len,
					kCFStringEncodingASCII, FALSE);
    if (serviceID == NULL) {
	return (ipconfig_status_allocation_failed_e);
    }
    status = S_remove_service_with_id_str(serviceID);
    CFRelease(serviceID);
    return (status);
}

PRIVATE_EXTERN ipconfig_status_t
find_service(const char * name, boolean_t exact,
	     ipconfig_method_t method,
	     ipconfig_method_data_t * method_data,
	     void * service_id, unsigned int * service_id_len)
{
    IFStateRef   	ifstate;
    unsigned int	in_length;
    CFIndex		len = 0;
    ServiceRef		service_p;

    in_length = *service_id_len;
    *service_id_len = 0;
    switch (method) {
    case ipconfig_method_none_e:
    case ipconfig_method_none_v4_e:
    case ipconfig_method_none_v6_e:
	return (ipconfig_status_invalid_parameter_e);
    default:
	break;
    }
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, name, NULL);
    if (ifstate == NULL) {
	return (ipconfig_status_interface_does_not_exist_e);
    }
    if (exact) {
	service_p 
	    = IFStateGetServiceWithMethod(ifstate, method,
					  method_data,
					  FALSE);
    }
    else {
	service_p
	    = IFStateGetServiceMatchingMethod(ifstate, method,
					      method_data,
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

PRIVATE_EXTERN ipconfig_status_t
remove_service(const char * name, ipconfig_method_t method,
	       ipconfig_method_data_t * method_data)
{
    IFStateRef   	ifstate;
    ServiceRef		service_p;

    switch (method) {
    case ipconfig_method_none_e:
    case ipconfig_method_none_v4_e:
    case ipconfig_method_none_v6_e:
	return (ipconfig_status_invalid_parameter_e);
    default:
	break;
    }
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, name, NULL);
    if (ifstate == NULL) {
	return (ipconfig_status_interface_does_not_exist_e);
    }
    service_p = IFStateGetServiceWithMethod(ifstate, method, method_data,
					    FALSE);
    if (service_p == NULL) {
	return (ipconfig_status_no_such_service_e);
    }
    return (S_remove_service(ifstate, service_p));
}

static boolean_t
ipconfig_method_from_cfstring(CFStringRef m, ipconfig_method_t * method)
{
    if (isA_CFString(m) == NULL) {
	return (FALSE);
    }
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

static ipconfig_status_t
method_info_from_dict(CFDictionaryRef dict,
		      ipconfig_method_t * ret_method,
		      ipconfig_method_data_t * * ret_method_data)
{
    ipconfig_method_t		method = ipconfig_method_none_e;
    CFStringRef			method_cf;
    ipconfig_method_data_t *	method_data = NULL;
    int				method_data_len = 0;
    boolean_t			status = ipconfig_status_invalid_parameter_e;

    method_cf = CFDictionaryGetValue(dict, 
				     kSCPropNetIPv4ConfigMethod);
    if (ipconfig_method_from_cfstring(method_cf, &method) == FALSE) {
	my_log(LOG_ERR,
	       "IPConfiguration: IPv4 ConfigMethod is missing/invalid");
	goto done;
    }
    if (ipconfig_method_is_manual(method)) {
	struct in_addr		address;
	CFArrayRef		addresses;
	CFStringRef		address_cf;
	int			count = 0;
	CFArrayRef		masks;
	CFStringRef		mask_cf = NULL;
	struct in_addr		mask = { 0 };
	
	addresses = isA_CFArray(CFDictionaryGetValue(dict,
						     kSCPropNetIPv4Addresses));
	masks = isA_CFArray(CFDictionaryGetValue(dict,
						 kSCPropNetIPv4SubnetMasks));
	if (addresses != NULL) {
	    count = CFArrayGetCount(addresses);
	}
	if (count == 0) {
	    my_log(LOG_ERR, 
		   "IPConfiguration: %s Addresses missing/invalid\n",
		   ipconfig_method_string(method));
	    goto done;
	}
	address_cf = CFArrayGetValueAtIndex(addresses, 0);
	if (my_CFStringToIPAddress(address_cf, &address) == FALSE) {
	    my_log(LOG_ERR, 
		   "IPConfiguration: %s Addresses invalid",
		   ipconfig_method_string(method));
	    goto done;
	}
	if (masks != NULL) {
	    if (count != CFArrayGetCount(masks)) {
		my_log(LOG_ERR, 
		       "IPConfiguration: "
		       "%s Addresses/SubnetMasks are different sizes",
		       ipconfig_method_string(method));
		goto done;
	    }
	    mask_cf = CFArrayGetValueAtIndex(masks, 0);
	    if (my_CFStringToIPAddress(mask_cf, &mask) == FALSE) {
		my_log(LOG_ERR, 
		       "IPConfiguration: %s SubnetMask invalid",
		       ipconfig_method_string(method));
		goto done;
	    }
	}
	if (count > 1) {
	    my_log(LOG_NOTICE, 
		   "IPConfiguration: %s "
		   "multiple addresses specified - ignoring all but first",
		   ipconfig_method_string(method));
	}
	method_data_len = sizeof(ipconfig_method_data_manual_t);
	method_data = (ipconfig_method_data_t *)malloc(method_data_len);
	if (method_data == NULL) {
	    my_log(LOG_ERR, "IPConfiguration: malloc method_data failed");
	    status = ipconfig_status_allocation_failed_e;
	    goto done;
	}
	bzero(method_data, method_data_len);
	method_data->manual.addr = address;
	method_data->manual.mask = mask;
	if (method == ipconfig_method_manual_e) {
	    CFBooleanRef	b;
	    CFStringRef		router = NULL;

	    b = isA_CFBoolean(CFDictionaryGetValue(dict,
						   kSCPropNetIgnoreLinkStatus));
	    method_data->manual.ignore_link_status
		= (b != NULL) ? CFBooleanGetValue(b) : FALSE;
	    router = CFDictionaryGetValue(dict, kSCPropNetIPv4Router);
	    if (router != NULL
		&& my_CFStringToIPAddress(router, &method_data->manual.router)
		== FALSE) {
		my_log(LOG_NOTICE,
		       "IPConfiguration: %s Router invalid",
		       ipconfig_method_string(method));
	    }
	}
	else if (method == ipconfig_method_failover_e) {
	    CFNumberRef	num;

	    num = CFDictionaryGetValue(dict,
				       kSCPropNetIPv4FailoverAddressTimeout);
	    if (num != NULL
		&& (isA_CFNumber(num) == NULL
		    || (CFNumberGetValue(num, kCFNumberSInt32Type,
					 &method_data->manual.failover_timeout)
			== FALSE))) {
		my_log(LOG_NOTICE,
		       "IPConfiguration: FailoverAddressTimeout invalid");
	    }
	}
    }
    else if (method == ipconfig_method_dhcp_e) {
	char			cid[255];
	int			cid_len = 0;
	CFStringRef		client_id = NULL;

	client_id = CFDictionaryGetValue(dict, kSCPropNetIPv4DHCPClientID);
	if (isA_CFString(client_id) != NULL) {
	    cid_len = my_CFStringToCStringAndLength(client_id, cid, sizeof(cid));
	}
	if (cid_len != 0) {
	    cid_len--; /* we don't want the trailing nul character */
	    method_data_len = offsetof(ipconfig_method_data_dhcp_t, client_id)
		+ cid_len;
	}
	if (method_data_len > 0) {
	    method_data = (ipconfig_method_data_t *)malloc(method_data_len);
	    if (method_data == NULL) {
		my_log(LOG_ERR, "IPConfiguration: malloc DHCPClientID failed");
		status = ipconfig_status_allocation_failed_e;
		goto done;
	    }
	    method_data->dhcp.client_id_len = cid_len;
	    bcopy(cid, method_data->dhcp.client_id, cid_len);
	}
    }
    status = ipconfig_status_success_e;

 done:
    *ret_method_data = method_data;
    *ret_method = method;
    return (status);
}

static boolean_t
ipconfig_method_from_cfstring_ipv6(CFStringRef m, ipconfig_method_t * method)
{
    if (isA_CFString(m) == NULL) {
	return (FALSE);
    }
    if (CFEqual(m, kSCValNetIPv6ConfigMethodManual)) {
	*method = ipconfig_method_manual_v6_e;
    }
    else if (CFEqual(m, kSCValNetIPv6ConfigMethodAutomatic)) {
	*method = ipconfig_method_automatic_v6_e;
    }
    else if (CFEqual(m, kSCValNetIPv6ConfigMethodRouterAdvertisement)) {
	*method = ipconfig_method_rtadv_e;
    }
    else if (CFEqual(m, kSCValNetIPv6ConfigMethod6to4)) {
	*method = ipconfig_method_stf_e;
    }
    else if (CFEqual(m, kSCValNetIPv6ConfigMethodLinkLocal)) {
	*method = ipconfig_method_linklocal_v6_e;
    }
    else {
	return (FALSE);
    }
    return (TRUE);
}

static bool
my_CFStringToIPv6Address(CFStringRef str, struct in6_addr * ret_ip)
{
    char		buf[64];

    if (isA_CFString(str) == NULL) {
	return (FALSE);
    }
    if (CFStringGetCString(str, buf, sizeof(buf), kCFStringEncodingASCII)
	== FALSE) {
	return (FALSE);
    }
    if (inet_pton(AF_INET6, buf, ret_ip) == 1) {
	return (TRUE);
    }
    return (FALSE);
}

static ipconfig_status_t
method_info_from_ipv6_dict(CFDictionaryRef dict,
			   ipconfig_method_t * ret_method,
			   ipconfig_method_data_t * * ret_method_data)
{
    ipconfig_method_t		method = ipconfig_method_none_v6_e;
    CFStringRef			method_cf;
    ipconfig_method_data_t *	method_data = NULL;
    int				method_data_len = 0;
    boolean_t			status = ipconfig_status_invalid_parameter_e;

    method_cf = CFDictionaryGetValue(dict, 
				     kSCPropNetIPv6ConfigMethod);
    if (ipconfig_method_from_cfstring_ipv6(method_cf, &method) == FALSE) {
	my_log(LOG_ERR,
	       "IPConfiguration: IPv6 ConfigMethod is missing/invalid");
	goto done;
    }
    if (method == ipconfig_method_manual_v6_e) {
	struct in6_addr		address;
	CFArrayRef		addresses;
	CFStringRef		address_cf;
	int			count = 0;
	CFArrayRef		prefixes;
	CFNumberRef		prefix_cf = NULL;
	int			prefix = 0;
	
	addresses 
	    = isA_CFArray(CFDictionaryGetValue(dict, kSCPropNetIPv6Addresses));
	prefixes 
	    = isA_CFArray(CFDictionaryGetValue(dict,
					       kSCPropNetIPv6PrefixLength));
	if (addresses != NULL) {
	    count = CFArrayGetCount(addresses);
	}
	if (count == 0) {
	    my_log(LOG_ERR, 
		   "IPConfiguration: %s Addresses missing/invalid\n",
		   ipconfig_method_string(method));
	    goto done;
	}
	address_cf = CFArrayGetValueAtIndex(addresses, 0);
	if (my_CFStringToIPv6Address(address_cf, &address) == FALSE) {
	    my_log(LOG_ERR, 
		   "IPConfiguration: %s Addresses invalid",
		   ipconfig_method_string(method));
	    goto done;
	}
	if (IN6_IS_ADDR_LINKLOCAL(&address)) {
	    my_log(LOG_ERR,
		   "IPConfiguration: %s cannot configure IPv6 Link Local address",
		   ipconfig_method_string(method));
	    goto done; 
	}
	if (prefixes != NULL) {
	    if (count != CFArrayGetCount(prefixes)) {
		my_log(LOG_ERR, 
		       "IPConfiguration: "
		       "%s Addresses/PrefixLength are different sizes",
		       ipconfig_method_string(method));
		goto done;
	    }
	    prefix_cf = CFArrayGetValueAtIndex(prefixes, 0);
	    if (isA_CFNumber(prefix_cf) == NULL
		|| (CFNumberGetValue(prefix_cf, kCFNumberIntType, &prefix)
		    == FALSE)) {
		my_log(LOG_ERR, "IPConfiguration: %s PrefixLength invalid",
		       ipconfig_method_string(method));
		goto done;
	    }
	}
	if (count > 1) {
	    my_log(LOG_NOTICE, 
		   "IPConfiguration: %s "
		   "multiple addresses specified - ignoring all but first",
		   ipconfig_method_string(method));
	}
	method_data_len = sizeof(ipconfig_method_data_manual_v6_t);
	method_data = (ipconfig_method_data_t *)malloc(method_data_len);
	if (method_data == NULL) {
	    my_log(LOG_ERR, "IPConfiguration: malloc method_data failed");
	    status = ipconfig_status_allocation_failed_e;
	    goto done;
	}
	bzero(method_data, method_data_len);
	method_data->manual_v6.addr = address;
	method_data->manual_v6.prefix_length = prefix;
    }
    else if (method == ipconfig_method_stf_e) {
	CFStringRef	relay_cf;

	relay_cf = CFDictionaryGetValue(dict, kSCPropNetIPv66to4Relay);
	if (relay_cf != NULL) {
	    char		buf[256];
	    int			len;
	    address_type_t	relay_addr_type;
	    struct in_addr	relay_ip;
	    struct in6_addr	relay_ipv6;

	    if (isA_CFString(relay_cf) == NULL) {
		my_log(LOG_ERR, "IPConfiguration: %s 6to4 Relay invalid",
		       ipconfig_method_string(method));
		goto done;
	    }
	    len = my_CFStringToCStringAndLength(relay_cf, buf, sizeof(buf));
	    if (len == 0) {
		my_log(LOG_ERR, "IPConfiguration: %s 6to4 Relay empty",
		       ipconfig_method_string(method));
		goto done;
	    }
	    if (inet_aton(buf, &relay_ip) == 1) {
		relay_addr_type = address_type_ipv4_e;
		method_data_len = sizeof(ipconfig_method_data_stf_t);
	    }
	    else if (inet_pton(AF_INET6, buf, &relay_ipv6) == 1) {
		relay_addr_type = address_type_ipv6_e;
		method_data_len = sizeof(ipconfig_method_data_stf_t);
	    }
	    else {
		relay_addr_type = address_type_dns_e;
		method_data_len = offsetof(ipconfig_method_data_stf_t,
					   relay_addr.dns) + len + 1;
	    }
	    method_data = (ipconfig_method_data_t *)malloc(method_data_len);
	    if (method_data == NULL) {
		my_log(LOG_ERR, "IPConfiguration: malloc method_data failed");
		status = ipconfig_status_allocation_failed_e;
		goto done;
	    }
	    method_data->stf.relay_addr_type = relay_addr_type;
	    switch (relay_addr_type) {
	    case address_type_ipv4_e:
		method_data->stf.relay_addr.v4 = relay_ip;
		break;
	    case address_type_ipv6_e:
		method_data->stf.relay_addr.v6 = relay_ipv6;
		break;
	    case address_type_dns_e:
	    default:
		bcopy(buf, method_data->stf.relay_addr.dns, len);
		method_data->stf.relay_addr.dns[len] = '\0';
		break;
	    }
	}
    }
    status = ipconfig_status_success_e;

 done:
    *ret_method_data = method_data;
    *ret_method = method;
    return (status);
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

static CFDictionaryRef
copy_ipv4_service_dict(CFDictionaryRef values, CFStringRef serviceID,
		       CFStringRef type, CFStringRef ifn_cf)
{
    CFDictionaryRef		dict;
    CFStringRef			key;
    CFMutableDictionaryRef	service_dict;

    if (CFEqual(type, kSCValNetInterfaceTypeEthernet) == FALSE
	&& CFEqual(type, kSCValNetInterfaceTypeFireWire) == FALSE
	&& CFEqual(type, kSCValNetInterfaceTypeLoopback) == FALSE) {
	/* we only configure ethernet/firewire/loopback interfaces currently */
	return (NULL);
    }
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      serviceID,
						      kSCEntNetIPv4);
    dict = CFDictionaryGetValue(values, key);
    CFRelease(key);
    if (isA_CFDictionary(dict) == NULL) {
	return (NULL);
    }
    /* return IPv4 dict annotated with interface name and serviceID */
    service_dict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
    CFDictionarySetValue(service_dict, kSCPropNetInterfaceDeviceName, 
			 ifn_cf);
    CFDictionarySetValue(service_dict, PROP_SERVICEID, serviceID);
    return (service_dict);
}

static CFDictionaryRef
copy_ipv6_service_dict(CFDictionaryRef values, CFStringRef serviceID,
		       CFStringRef type, CFStringRef ifn_cf)
{
    CFDictionaryRef		dict;
    CFStringRef			key;
    CFMutableDictionaryRef	service_dict;

    if (CFEqual(type, kSCValNetInterfaceTypeEthernet) == FALSE
	&& CFEqual(type, kSCValNetInterfaceTypeFireWire) == FALSE
	&& CFEqual(type, kSCValNetInterfaceType6to4) == FALSE
	&& CFEqual(type, kSCValNetInterfaceTypeLoopback) == FALSE) {
	/* we only configure ethernet/firewire/6to4/loopback interfaces currently */
	return (NULL);
    }
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      serviceID,
						      kSCEntNetIPv6);
    dict = CFDictionaryGetValue(values, key);
    CFRelease(key);
    
    if (isA_CFDictionary(dict) == NULL) {
	return (NULL);
    }
    /* return IPv6 dict annotated with interface name, serviceID, 6to4Relay */
    service_dict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
    CFDictionarySetValue(service_dict, kSCPropNetInterfaceDeviceName, 
			 ifn_cf);
    CFDictionarySetValue(service_dict, PROP_SERVICEID, serviceID);

    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      serviceID,
						      kSCEntNet6to4);
    dict = CFDictionaryGetValue(values, key);
    CFRelease(key);
    if (isA_CFDictionary(dict) != NULL) {
	CFStringRef		stf_relay;
	
	stf_relay = CFDictionaryGetValue(dict, kSCPropNet6to4Relay);
	if (stf_relay != NULL) {
	    CFDictionarySetValue(service_dict,
				 kSCPropNetIPv66to4Relay,
				 stf_relay);
	}
    }
    return (service_dict);
}

static CFArrayRef
copy_serviceIDs_from_values(CFDictionaryRef values, CFArrayRef order_array)
{
    int			count;
    int			i;
    const void * *	keys;
    CFMutableArrayRef	list = NULL;
    int			list_count;

    /* if there are no values, we're done */
    count = CFDictionaryGetCount(values);
    if (count == 0) {
	return (NULL);
    }
    list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    keys = (const void * *)malloc(sizeof(*keys) * count);
    CFDictionaryGetKeysAndValues(values, (const void * *)keys, NULL);
    for (i = 0; i < count; i++) {
	CFStringRef		serviceID;
	
	if (CFStringHasPrefix(keys[i], S_setup_service_prefix) == FALSE) {
	    continue;
	}
	/* Setup:/Network/Service/<serviceID>/{IPv4,[IPv6,]Interface} */
	serviceID = my_CFStringCopyComponent(keys[i], CFSTR("/"), 3);
	if (serviceID == NULL) {
	    continue;
	}
	my_CFArrayAppendUniqueValue(list, serviceID);
	CFRelease(serviceID);
    }
    free(keys);
    list_count = CFArrayGetCount(list);
    if (list_count == 0) {
	my_CFRelease(&list);
    }
    else if (order_array != NULL) {
	/* sort the list according to the defined service order */
	CFArraySortValues(list, CFRangeMake(0, list_count),
			  compare_serviceIDs, (void *)order_array);
    }
    return (list);
}

static CFArrayRef
entity_all(SCDynamicStoreRef session, CFArrayRef * ret_ipv6_services)
{
    CFMutableArrayRef		all_services = NULL;
    CFMutableArrayRef		all_v6_services = NULL;
    CFMutableArrayRef		get_keys = NULL;
    CFMutableArrayRef		get_patterns = NULL;
    int				i;
    CFStringRef			key = NULL;
    CFArrayRef			service_IDs = NULL;
    int				service_IDs_count;
    CFStringRef			order_key = NULL;
    CFArrayRef			order_array = NULL;
    CFDictionaryRef		values = NULL;

    get_keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    get_patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    all_services = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    all_v6_services = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    /* get Setup:/Network/Service/any/{IPv4,[ IPv6,] Interface} */
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      kSCCompAnyRegex,
						      kSCEntNetIPv4);
    CFArrayAppendValue(get_patterns, key);
    CFRelease(key);

    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      kSCCompAnyRegex,
						      kSCEntNetIPv6);
    CFArrayAppendValue(get_patterns, key);
    CFRelease(key);
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      kSCCompAnyRegex,
						      kSCEntNet6to4);
    CFArrayAppendValue(get_patterns, key);
    CFRelease(key);

    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      kSCCompAnyRegex,
						      kSCEntNetInterface);
    CFArrayAppendValue(get_patterns, key);
    CFRelease(key);

    /* populate keys array to get Setup:/Network/Global/IPv4 */
    order_key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							   kSCDynamicStoreDomainSetup,
							   kSCEntNetIPv4);
    CFArrayAppendValue(get_keys, order_key);

    /* get keys and values atomically */
    values = SCDynamicStoreCopyMultiple(session, get_keys, get_patterns);
    if (values == NULL) {
	goto done;
    }

    /* grab the service order array */
    order_array = get_order_array_from_values(values, order_key);

    /* build a list of configured service ID's */
    service_IDs = copy_serviceIDs_from_values(values, order_array);
    if (service_IDs == NULL) {
	/* if there are no serviceIDs, we're done */
	goto done;
    }

    /* populate all_services array with annotated IPv4[/IPv6] dict's */
    service_IDs_count = CFArrayGetCount(service_IDs);
    for (i = 0; i < service_IDs_count; i++) {
	CFStringRef 		key;
	CFDictionaryRef		if_dict;
	CFStringRef 		ifname;
	CFDictionaryRef		service_dict = NULL;
	CFStringRef		serviceID;
	CFStringRef		type;
	
	serviceID = CFArrayGetValueAtIndex(service_IDs, i);
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainSetup,
							  serviceID,
							  kSCEntNetInterface);
	if_dict = CFDictionaryGetValue(values, key);
	CFRelease(key);
	if_dict = isA_CFDictionary(if_dict);
	if (if_dict == NULL) {
	    continue;
	}
	type = CFDictionaryGetValue(if_dict, kSCPropNetInterfaceType);
	if (isA_CFString(type) == NULL) {
	    SCLog(TRUE, LOG_NOTICE, 
		  CFSTR("IPConfiguration: Interface Type missing/invalid"
			"\nInterface = %@"), if_dict);
	    continue;
	}
	ifname = CFDictionaryGetValue(if_dict, kSCPropNetInterfaceDeviceName);
	if (isA_CFString(ifname) == NULL) {
	    continue;
	}

	/* get IPv4 service configuration */
	service_dict = copy_ipv4_service_dict(values, serviceID,
					      type, ifname);
	if (service_dict != NULL) {
	    CFArrayAppendValue(all_services, service_dict);
	    CFRelease(service_dict);
	}
	/* get IPv6 service configuration */
	service_dict = copy_ipv6_service_dict(values, serviceID,
					      type, ifname);
	if (service_dict != NULL) {
	    CFArrayAppendValue(all_v6_services, service_dict);
	    CFRelease(service_dict);
	}
    }

 done:
    my_CFRelease(&values);
    my_CFRelease(&order_key);
    my_CFRelease(&get_keys);
    my_CFRelease(&get_patterns);
    my_CFRelease(&service_IDs);
    if (all_services != NULL && CFArrayGetCount(all_services) == 0) {
	my_CFRelease(&all_services);
    }
    if (all_v6_services != NULL && CFArrayGetCount(all_v6_services) == 0) {
	my_CFRelease(&all_v6_services);
    }
    *ret_ipv6_services = all_v6_services;
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
} ServiceConfig, * ServiceConfigRef;

typedef struct {
    ServiceConfigRef		list;
    int				count;
    boolean_t			is_ipv4;
} ServiceConfigList, * ServiceConfigListRef;

static void
ServiceConfigListFree(ServiceConfigListRef scl)
{
    int 		i;
    ServiceConfigRef	scan;

    if (scl->list == NULL) {
	return;
    }
    for (i = 0, scan = scl->list; i < scl->count; i++, scan++) {
	my_CFRelease(&scan->serviceID);
	if (scan->method_data != NULL) {
	    free(scan->method_data);
	}
    }
    free(scl->list);
    scl->list = NULL;
    return;
}

#ifdef DEBUG
void
ServiceConfigListPrint(ServiceConfigListRef scl)
{
    int			i;
    ServiceConfigRef	scan;

    my_log(LOG_NOTICE, 
	   "%d %s configs\n", scl->count, scl->is_ipv4 ? "IPv4" : "IPv6");
    for (i = 0, scan = scl->list; i < scl->count; i++, scan++) {
	SCLog(TRUE, LOG_NOTICE, CFSTR("serviceID %@ %s Data %p"),
	      scan->serviceID,
	      ipconfig_method_string(scan->method),
	      scan->method_data);
	if (scan->method_data != NULL 
	    && scan->method == ipconfig_method_stf_e) {
	    char 		ntopbuf[INET6_ADDRSTRLEN];
    	    ipconfig_method_data_stf_t * stf;
	    stf = &scan->method_data->stf;
	    switch (stf->relay_addr_type) {
	    case address_type_ipv4_e:
		my_log(LOG_NOTICE, "IPv4 relay " IP_FORMAT,
		       IP_LIST(&stf->relay_addr.v4));
		break;
	    case address_type_ipv6_e:
		my_log(LOG_NOTICE, "IPv6 relay %s",
		       inet_ntop(AF_INET6, &stf->relay_addr.v6,
				 ntopbuf, sizeof(ntopbuf)));
		break;
	    case address_type_dns_e:
		my_log(LOG_NOTICE, "DNS relay %s", stf->relay_addr.dns);
		break;
	    default:
		my_log(LOG_NOTICE, "Bogus relay type %d", stf->relay_addr_type);
		break;
	    }
	}
    }
}
#endif /* DEBUG */

static ServiceConfigRef
ServiceConfigListLookupMethod(ServiceConfigListRef scl,
			      ipconfig_method_t method, 
			      ipconfig_method_data_t * method_data)
{
    int 		i;
    ServiceConfigRef	scan;

    switch (method) {
    case ipconfig_method_stf_e:
    case ipconfig_method_linklocal_e:
	for (i = 0, scan = scl->list; i < scl->count; i++, scan++) {
	    if (method == scan->method) {
		return (scan);
	    }
	}
	break;
    case ipconfig_method_rtadv_e:
    case ipconfig_method_automatic_v6_e:
	for (i = 0, scan = scl->list; i < scl->count; i++, scan++) {
	    switch (scan->method) {
	    case ipconfig_method_rtadv_e:
	    case ipconfig_method_automatic_v6_e:
		return (scan);
	    default:
		break;
	    }
	}
	break;
    case ipconfig_method_dhcp_e:
    case ipconfig_method_bootp_e:
	for (i = 0, scan = scl->list; i < scl->count; i++, scan++) {
	    if (ipconfig_method_is_dhcp_or_bootp(scan->method))
		return (scan);
	}
	break;
    case ipconfig_method_failover_e:
    case ipconfig_method_manual_e:
    case ipconfig_method_inform_e:
	for (i = 0, scan = scl->list; i < scl->count; i++, scan++) {
	    if (ipconfig_method_is_manual(scan->method)
		&& (method_data->manual.addr.s_addr
		    == scan->method_data->manual.addr.s_addr)) {
		return (scan);
	    }
	}
	break;
    case ipconfig_method_manual_v6_e:
	for (i = 0, scan = scl->list; i < scl->count; i++, scan++) {
	    if (scan->method != ipconfig_method_manual_v6_e) {
		continue;
	    }
	    if (IN6_ARE_ADDR_EQUAL(&method_data->manual_v6.addr,
				   &scan->method_data->manual_v6.addr)) {
		return (scan);
	    }
	}
	break;
    default:
	break;
    }
    return (NULL);
}

static ServiceConfigRef
ServiceConfigListLookupService(ServiceConfigListRef scl, CFStringRef serviceID)
{
    int 		i;
    ServiceConfigRef	scan;

    for (i = 0, scan = scl->list; i < scl->count; i++, scan++) {
	if (CFEqual(serviceID, scan->serviceID)) {
	    return (scan);
	}
    }
    return (NULL);
}

static ServiceRef
find_dynamic_service(const char * ifname, ipconfig_method_t method,
		     ipconfig_method_data_t * method_data)
{
    interface_t * 	if_p = ifl_find_name(S_interfaces, ifname);
    IFStateRef		ifstate = NULL;

    if (if_p == NULL) {
	return (NULL);
    }
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, ifname, NULL);
    if (ifstate == NULL) {
	return (NULL);
    }
    return (IFStateGetServiceMatchingMethod(ifstate, method, 
					    method_data, TRUE));
}

static boolean_t
ServiceConfigListInit(ServiceConfigListRef scl, boolean_t is_ipv4,
		      CFArrayRef all_services, const char * ifname)
{
    int			i;
    CFArrayRef 		if_service_list;
    int			if_service_count;
    CFStringRef		ifn_cf = NULL;
    boolean_t		ret;

    bzero(scl, sizeof(*scl));
    scl->is_ipv4 = is_ipv4;
    ifn_cf = CFStringCreateWithCString(NULL, ifname,
				       kCFStringEncodingASCII);
    if_service_list = interface_services_copy(all_services, ifn_cf);
    if (if_service_list == NULL) {
	goto done;
    }
    if_service_count = CFArrayGetCount(if_service_list);
    scl->list = (ServiceConfigRef)malloc(if_service_count * sizeof(*scl->list));
    if (scl->list == NULL) {
	goto done;
    }
    for (i = 0; i < if_service_count; i++) {
	boolean_t		duplicate_config = FALSE;
	boolean_t		duplicate_dynamic = FALSE;
	ipconfig_method_t	method;
	ipconfig_method_data_t *method_data = NULL;
	CFDictionaryRef		service_dict;
	CFStringRef		serviceID;

	service_dict = CFArrayGetValueAtIndex(if_service_list, i);
	if (is_ipv4) {
	    if (method_info_from_dict(service_dict, &method, &method_data)
		!= ipconfig_status_success_e) {
		continue;
	    }
	}
	else {
	    if (method_info_from_ipv6_dict(service_dict, &method, &method_data)
		!= ipconfig_status_success_e) {
		continue;
	    }
	}
	duplicate_config
	    = (ServiceConfigListLookupMethod(scl, method, method_data) != NULL);
	if (duplicate_config == FALSE) {
	    duplicate_dynamic = (find_dynamic_service(ifname, method, 
						      method_data) != NULL);
	}
	if (duplicate_config || duplicate_dynamic) {
	    my_log(LOG_NOTICE, "%s: %s %s",
		   ifname, ipconfig_method_string(method),
		   duplicate_config 
		   ? "duplicate configured service" 
		   : "configured service conflicts with dynamic service");
	    free(method_data);
	    continue;
	}
	serviceID = CFDictionaryGetValue(service_dict, PROP_SERVICEID);
	scl->list[scl->count].serviceID = CFRetain(serviceID);
	scl->list[scl->count].method = method;
	scl->list[scl->count].method_data = method_data;
	scl->count++;
    }
 done:
    if (scl->count == 0) {
	ServiceConfigListFree(scl);
	ret = FALSE;
    }
    else {
	ret = TRUE;
    }
    my_CFRelease(&ifn_cf);
    my_CFRelease(&if_service_list);
    return (ret);
}

static void
ServiceConfigListFreeInactiveServices(ServiceConfigListRef scl,
				      const char * ifname)
{
    CFMutableArrayRef	inactive_list = NULL;
    int			inactive_list_count;
    int			i;
    IFStateRef		ifstate;
    dynarray_t *	list;

    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, ifname, NULL);
    if (ifstate == NULL) {
	return;
    }
    if (scl->is_ipv4) {
	list = &ifstate->services;
    }
    else {
	list = &ifstate->services_v6;
    }
    inactive_list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (inactive_list == NULL) {
	return;
    }
    for (i = 0; i < dynarray_count(list); i++) {
	ServiceRef	service_p = dynarray_element(list, i);
	CFStringRef 	serviceID = service_p->serviceID;

	if (service_p->is_dynamic) {
	    /* dynamically created services survive configuration changes */
	    continue;
	}
	if (service_p->parent_serviceID != NULL) {
	    /* this service gets cleaned up on its own */
	    continue;
	}
	if (ServiceConfigListLookupService(scl, serviceID) == NULL) {
	    CFArrayAppendValue(inactive_list, serviceID);
	}
    }
    inactive_list_count = CFArrayGetCount(inactive_list);
    for (i = 0; i < inactive_list_count; i++) {
	CFStringRef serviceID = CFArrayGetValueAtIndex(inactive_list, i);

	IFStateFreeServiceWithID(ifstate, serviceID, scl->is_ipv4);
    }
    my_CFRelease(&inactive_list);
    return;
}

static ipconfig_status_t
S_set_service(IFStateRef ifstate, ServiceConfigRef config, boolean_t is_ipv4)
{
    CFStringRef		serviceID = config->serviceID;
    ServiceRef		service_p;
    IFStateRef		this_ifstate = NULL;

    service_p = IFStateGetServiceWithID(ifstate, serviceID, is_ipv4);
    if (service_p != NULL) {
	boolean_t		needs_stop = FALSE;
	ipconfig_status_t	status;

	if (service_p->method == config->method) {
	    status = config_method_change(service_p, config->method, 
					  config->method_data,
					  &needs_stop);
	    if (status == ipconfig_status_success_e
		&& needs_stop == FALSE) {
		return (ipconfig_status_success_e);
	    }
	}
	IFStateFreeService(ifstate, service_p);
    }
    else {
	this_ifstate = IFStateListGetServiceWithID(&S_ifstate_list, 
						   serviceID,
						   &service_p,
						   is_ipv4);
	if (this_ifstate) {
	    /* service is on other interface, stop it now */
	    IFStateFreeService(this_ifstate, service_p);
	}
    }
    return (IFState_service_add(ifstate, serviceID, config->method,
				config->method_data,
				NULL, NULL));
}

static void
interface_configuration_changed(interface_t * if_p, CFArrayRef all,
				boolean_t is_ipv4)
{
    IFStateRef		ifstate;
    ServiceConfigList	scl;

    /* if no services are defined, remove them all */
    if (ServiceConfigListInit(&scl, is_ipv4, all, if_name(if_p)) == FALSE) {
	ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, 
						if_name(if_p), NULL);
	if (ifstate != NULL) {
	    if (is_ipv4) {
		IFStateFreeIPv4Services(ifstate, FALSE);
	    }
	    else {
		IFStateFreeIPv6Services(ifstate, FALSE);
	    }
	}
	return;
    }

    /* stop services that are no longer active */
    ServiceConfigListFreeInactiveServices(&scl, if_name(if_p));

    /* update services that are still defined */
    ifstate = IFStateList_ifstate_create(&S_ifstate_list, if_p);
    if (ifstate != NULL) {
	int			i;
	ServiceConfigRef	config;
	
	/* update each of the services that are configured */
	for (i = 0, config = scl.list; i < scl.count; i++, config++) {
	    (void)S_set_service(ifstate, config, scl.is_ipv4);
	}
    }
    ServiceConfigListFree(&scl);
    return;
}

static void
handle_configuration_changed(SCDynamicStoreRef session,
			     CFArrayRef all_ipv4, CFArrayRef all_ipv6)
{
    int i;

    for (i = 0; i < ifl_count(S_interfaces); i++) {
	interface_t *		if_p = ifl_at_index(S_interfaces, i);

	interface_configuration_changed(if_p, all_ipv4, IS_IPV4);
	interface_configuration_changed(if_p, all_ipv6, IS_IPV6);
    }
    return;
}

static void
configuration_changed(SCDynamicStoreRef session)
{
    CFArrayRef		all_ipv4 = NULL;
    CFArrayRef		all_ipv6 = NULL;

    all_ipv4 = entity_all(session, &all_ipv6);
    handle_configuration_changed(session, all_ipv4, all_ipv6);
    my_CFRelease(&all_ipv4);
    my_CFRelease(&all_ipv6);
    return;
}

static void
configure_from_cache(SCDynamicStoreRef session)
{
    CFArrayRef		all_ipv4 = NULL;
    CFArrayRef		all_ipv6 = NULL;
    int			count = 0;
    int 		i;

    all_ipv4 = entity_all(session, &all_ipv6);
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

	ifn_cf = CFStringCreateWithCString(NULL,
					   if_name(if_p),
					   kCFStringEncodingASCII);
	if (ifn_cf == NULL) {
	    continue;
	}
	dict = lookup_entity(all_ipv4, ifn_cf);
	if (dict != NULL) {
	    (void)IFStateList_ifstate_create(&S_ifstate_list, if_p);
	    count++;
	}
	CFRelease(ifn_cf);
    }

 done:
    if (count == 0) {
	unblock_startup(session);
    }
    else {
	handle_configuration_changed(session, all_ipv4, all_ipv6);
    }
    my_CFRelease(&all_ipv4);
    my_CFRelease(&all_ipv6);

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
    CFRelease(key);
    /* notify when IPv6 config of any service changes */
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      kSCCompAnyRegex,
						      kSCEntNetIPv6);
    CFArrayAppendValue(patterns, key);
    CFRelease(key);
    /* notify when 6to4 config of any service changes */
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      kSCCompAnyRegex,
						      kSCEntNet6to4);
    CFArrayAppendValue(patterns, key);
    CFRelease(key);
    /* notify when IPv6 address changes on any interface */
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							kSCCompAnyRegex,
							kSCEntNetIPv6);
    CFArrayAppendValue(patterns, key);
    CFRelease(key);
    /* notify when Interface service <-> interface binding changes */
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      kSCCompAnyRegex,
						      kSCEntNetInterface);
    CFArrayAppendValue(patterns, key);
    CFRelease(key);

    /* notify when the link status of any interface changes */
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							kSCCompAnyRegex,
							kSCEntNetLink);
    CFArrayAppendValue(patterns, key);
    CFRelease(key);

    /* notify when the bssid key of any interface changes */
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							kSCCompAnyRegex,
							kSCEntNetAirPort);
    CFArrayAppendValue(patterns, key);
    CFRelease(key);

    /* notify for a refresh configuration request */
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							kSCCompAnyRegex,
							kSCEntNetRefreshConfiguration);
    CFArrayAppendValue(patterns, key);
    CFRelease(key);

    /* notify when there's an ARP collision on any interface */
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							kSCCompAnyRegex,
							kSCEntNetIPv4ARPCollision);
    pattern = CFStringCreateMutableCopy(NULL, 0, key);
    CFStringAppend(pattern, CFSTR(".*"));
    CFRelease(key);
    CFArrayAppendValue(patterns, pattern);
    CFRelease(pattern);

    /* notify when list of interfaces changes */
    key = SCDynamicStoreKeyCreateNetworkInterface(NULL,
						  kSCDynamicStoreDomainState);
    CFArrayAppendValue(keys, key);
    CFRelease(key);

    /* notify when the service order changes */
    key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainSetup,
						     kSCEntNetIPv4);
    CFArrayAppendValue(keys, key);
    CFRelease(key);

    /* notify when ComputerName/LocalHostName changes */
    S_computer_name_key = SCDynamicStoreKeyCreateComputerName(NULL);
    CFArrayAppendValue(keys, S_computer_name_key);
    S_hostnames_key = SCDynamicStoreKeyCreateHostNames(NULL);
    CFArrayAppendValue(keys, S_hostnames_key);

    SCDynamicStoreSetNotificationKeys(session, keys, patterns);
    CFRelease(keys);
    CFRelease(patterns);

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
	my_log(LOG_ERR, "IPConfiguration: ifl_init failed");
	return (FALSE);
    }
    if (S_interfaces) {
	ifl_free(&S_interfaces);
    }
    S_interfaces = new_interfaces;

    return (TRUE);
}

interface_list_t *
get_interface_list(void)
{
    if (S_interfaces == NULL) {
	S_interfaces = ifl_init();
    }
    return (S_interfaces);
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
	IFStateRef	ifstate = dynarray_element(&S_ifstate_list, i);
	
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
runloop_observer(CFRunLoopObserverRef observer, 
		 CFRunLoopActivity activity, void *info)
{
    if (S_scd_session == NULL) {
	return;
    }
    if (S_linklocal_needs_attention) {
	CFArrayRef		service_order = NULL;

	S_linklocal_needs_attention = FALSE;
	service_order = S_get_service_order(S_scd_session);
	if (G_IPConfiguration_verbose) {
	    my_log(LOG_NOTICE, "runloop_observer: calling S_linklocal_elect");
	}
	S_linklocal_elect(service_order);
	my_CFRelease(&service_order);
    }
    my_SCDynamicStorePublish(S_scd_session);
    return;
}

/*
 * Function: woke_from_hibernation
 *
 * Purpose:
 *   When we wake from sleep, check whether we woke from a hibernation
 *   image or a regular wake from sleep.
 */

#define IO_PATH_PM_ROOT_DOMAIN kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain"
STATIC boolean_t
woke_from_hibernation(void)
{
    CFDataRef		hib_prop;
    uint32_t		hibernate_state;

    hibernate_state = kIOHibernateStateInactive;
    hib_prop = myIORegistryEntryCopyProperty(IO_PATH_PM_ROOT_DOMAIN,
					     CFSTR(kIOHibernateStateKey)); 
    if (isA_CFData(hib_prop) != NULL 
	&& CFDataGetLength(hib_prop) == sizeof(hibernate_state)) {
	/* ALIGN: CFDataGetBytePtr aligns to at least sizeof(uint64), cast safe */
	hibernate_state = *((uint32_t *)(void *)CFDataGetBytePtr(hib_prop));
    }
    my_CFRelease(&hib_prop);
    return (hibernate_state == kIOHibernateStateWakingFromHibernate);
}

STATIC void
S_deliver_wake_event(void)
{
    WakeFlags		flags = 0;
    wake_data_t		event_data;
    int 		i;
    int			if_count = dynarray_count(&S_ifstate_list);

    if (woke_from_hibernation()) {
	flags |= kWakeFlagsFromHibernation;
    }
    for (i = 0; i < if_count; i++) {
	IFStateRef		ifstate = dynarray_element(&S_ifstate_list, i);

	event_data.flags = flags;
	if (if_is_wireless(ifstate->if_p)) {
	    CFStringRef		ssid;
	    struct ether_addr	bssid;

	    ssid = S_copy_ssid_bssid(ifstate->ifname, &bssid);
	    if (ssid != NULL
		&& ifstate->ssid != NULL) {
		if (!CFEqual(ssid, ifstate->ssid)) {
		    event_data.flags |= kWakeFlagsSSIDChanged;
		}
		else if (bcmp(&bssid, &ifstate->bssid, sizeof(bssid))) {
		    event_data.flags |= kWakeFlagsBSSIDChanged;
		}
		IFState_set_ssid_bssid(ifstate, ssid, &bssid);
	    }
	    my_CFRelease(&ssid);
	}
	if (dynarray_count(&ifstate->services) > 0) {
	    /* attach IPv4 in case the interface went away during sleep */
	    inet_attach_interface(if_name(ifstate->if_p));
	    service_list_event(&ifstate->services, IFEventID_wake_e,
			       (void *)&event_data);
	}
	if (dynarray_count(&ifstate->services_v6) > 0) {
	    /* attach IPv6 in case the interface went away during sleep */
	    inet6_attach_interface(if_name(ifstate->if_p));
	    service_list_event(&ifstate->services_v6, IFEventID_wake_e,
			       (void *)&event_data);
	}
    }
    return;
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
	IFStateList_all_services_event(&S_ifstate_list,
				       IFEventID_sleep_e, NULL);
	break;

    case kIOMessageSystemWillPowerOn:
	my_log(LOG_DEBUG, "IPConfiguration: Wake");
	S_deliver_wake_event();
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
	    S_deliver_wake_event();
	}
    }
    else {
	/* sleep */
	S_awake = FALSE;
	my_log(LOG_DEBUG, "IPConfiguration: Sleep");
	IFStateList_all_services_event(&S_ifstate_list,
				       IFEventID_sleep_e, NULL);

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

    S_observer = CFRunLoopObserverCreate(NULL,
					 kCFRunLoopAllActivities,
					 TRUE, 0, runloop_observer, NULL);
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
    IFStateRef   		ifstate;
    int 			j;
    
    if (CFStringHasPrefix(cache_key, S_state_interface_prefix) == FALSE) {
	return;
    }
    /* State:/Network/Interface/<ifname>/RefreshConfiguration */
    ifn_cf = my_CFStringCopyComponent(cache_key, CFSTR("/"), 3);
    if (ifn_cf == NULL) {
	return;
    }
    my_CFStringToCStringAndLength(ifn_cf, ifn, sizeof(ifn));
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, ifn, NULL);
    if (ifstate == NULL || ifstate->netboot) {
	/* don't propagate media status events for netboot interface */
	goto done;
    }
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	ServiceRef	service_p = dynarray_element(&ifstate->services, j);

	config_method_renew(service_p);
    }
    service_list_event(&ifstate->services_v6, IFEventID_renew_e, NULL);

 done:
    my_CFRelease(&ifn_cf);
    return;
}

static void
ipv6_interface_address_changed(SCDynamicStoreRef session, CFStringRef cache_key)
{
    inet6_addrlist_t 	addr_list;
    CFStringRef		ifn_cf;
    char		ifn[IFNAMSIZ + 1];
    IFStateRef   	ifstate;
    
    if (CFStringHasPrefix(cache_key, S_state_interface_prefix) == FALSE) {
	return;
    }

    /* figure out which interface this belongs to and deliver the event */

    /* State:/Network/Interface/<ifname>/IPv6 */
    ifn_cf = my_CFStringCopyComponent(cache_key, CFSTR("/"), 3);
    if (ifn_cf == NULL) {
	return;
    }
    my_CFStringToCStringAndLength(ifn_cf, ifn, sizeof(ifn));
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, ifn, NULL);
    if (ifstate == NULL) {
	/* not tracking this event */
	goto done;
    }
    /* get the addresses from the interface and deliver the event */
    inet6_addrlist_copy(&addr_list, if_link_index(ifstate->if_p));
    service_list_event(&ifstate->services_v6,
		       IFEventID_ipv6_address_changed_e, &addr_list);
    inet6_addrlist_free(&addr_list);

 done:
    my_CFRelease(&ifn_cf);
    return;
}


#include "my_darwin.h"

#ifndef NO_WIRELESS

#include <Apple80211/Apple80211API.h>
#include <Kernel/IOKit/apple80211/apple80211_ioctl.h>

static CFStringRef
S_copy_ssid_bssid(CFStringRef ifname, struct ether_addr * ap_mac)
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
    if (ap_mac != NULL) {
	(void)Apple80211Get((Apple80211Ref)wref, APPLE80211_IOC_BSSID, 0, 
			    ap_mac, sizeof(*ap_mac));
    }

 done:
    Apple80211Close(wref);
    return (ssid_str);
}

#else /* NO_WIRELESS */

static CFStringRef
S_copy_ssid_bssid(CFStringRef ifname, struct ether_addr * ap_mac)
{
    return (NULL);
}

#endif /* NO_WIRELESS */

static void
link_timer_expired(void * arg0, void * arg1, void * arg2)
{
    IFStateRef	ifstate = (IFStateRef)arg0;

    if (G_IPConfiguration_verbose) {
	my_log(LOG_NOTICE, "%s: link inactive timer fired",
	       if_name(ifstate->if_p));
    }
    service_list_event(&ifstate->services,
		       IFEventID_link_timer_expired_e, NULL);
    service_list_event(&ifstate->services_v6,
		       IFEventID_link_timer_expired_e, NULL);
    if (dynarray_count(&ifstate->services_v6) != 0) {
	(void)inet6_linklocal_stop(if_name(ifstate->if_p));
    }
    return;
}

static void
ap_key_changed(SCDynamicStoreRef session, CFStringRef cache_key) 
{
    interface_t *               if_p = NULL;
    CFStringRef                 ifn_cf = NULL;
    char                        ifn[IFNAMSIZ + 1];
    IFStateRef                  ifstate;
    link_status_t               link;
    uint8_t                     network_changed  = NETWORK_CHANGED_NONE;
    uint32_t			j;
    struct ether_addr		bssid;
    CFStringRef 		ssid;

    if (CFStringHasPrefix(cache_key, S_state_interface_prefix) == FALSE) {
	return;
    }

    /* State:/Network/Interface/<ifname>/Airport */
    ifn_cf = my_CFStringCopyComponent(cache_key, CFSTR("/"), 3);
    if (ifn_cf == NULL) {
	return;
    }

    my_CFStringToCStringAndLength(ifn_cf, ifn, sizeof(ifn));
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, ifn, NULL);
    
    if (ifstate == NULL || ifstate->netboot) {
	/* don't propagate media status events for netboot interface */
	goto done;
    }

    if_p = ifl_find_name(S_interfaces, ifn);
    if (if_p == NULL) {
	/* interface doesn't exist */
	goto done;
    }

    link = if_link_status_update(if_p);

    /* Link is/was down, no need to handle it now.  This will be handled
     * when the next link up event comes in */
    if ((link.valid && link.active == FALSE) 
	|| (ifstate->if_p->link_status.valid 
	&& (ifstate->if_p->link_status.active == FALSE))) {
	goto done;
    }

    if (if_is_wireless(ifstate->if_p) == FALSE) {
	goto done;
    }

    ssid = S_copy_ssid_bssid(ifstate->ifname, &bssid);
    if (G_IPConfiguration_verbose) {
	if (ssid != NULL) {
	    SCLog(TRUE, LOG_NOTICE, CFSTR("%s: SSID %@ BSSID %s"),
		  ifn, ssid, ether_ntoa(&bssid));
	}
	else {
	    my_log(LOG_NOTICE, "%s: no SSID", ifn);
	}
    }
    if (ssid != NULL) {
	/* remember the last ssid */
	network_changed = IFState_set_ssid_bssid(ifstate, ssid, &bssid);
       
	if (G_IPConfiguration_verbose) {
	    my_log(LOG_NOTICE, "%s network changed flag 0x%x.",
		   ifn, network_changed);
	}
	CFRelease(ssid);
    }

    /* Proceed only if bssid has changed */   
    if (network_changed == NETWORK_CHANGED_NONE 
	|| network_changed & NETWORK_CHANGED_SSID) {
	goto done;
    }

    /* Notify v4 services */ 
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	ServiceRef      service_p = dynarray_element(&ifstate->services, j);

	config_method_bssid_changed(service_p);
    }

    /* Notify v6 services */
    service_list_event(&ifstate->services_v6, IFEventID_bssid_changed_e,
		       NULL);

    done:
	my_CFRelease(&ifn_cf);
	return;
}

static void
link_key_changed(SCDynamicStoreRef session, CFStringRef cache_key)
{
    CFDictionaryRef		dict = NULL;
    interface_t *		if_p = NULL;
    CFStringRef			ifn_cf = NULL;
    char			ifn[IFNAMSIZ + 1];
    IFStateRef   		ifstate;
    int 			j;
    link_status_t		link;
    void *			network_changed = NULL;

    if (CFStringHasPrefix(cache_key, S_state_interface_prefix) == FALSE) {
	return;
    }
    /* State:/Network/Interface/<ifname>/Link */
    ifn_cf = my_CFStringCopyComponent(cache_key, CFSTR("/"), 3);
    if (ifn_cf == NULL) {
	return;
    }
    my_CFStringToCStringAndLength(ifn_cf, ifn, sizeof(ifn));
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, ifn, NULL);
    dict = my_SCDynamicStoreCopyDictionary(session, cache_key);
    if (dict != NULL) {
	if (CFDictionaryContainsKey(dict, kSCPropNetLinkDetaching)) {
	    if (ifstate != NULL) {
		IFStateFreeIPv4Services(ifstate, TRUE);
		IFStateFreeIPv6Services(ifstate, TRUE);
	    }
	    goto done;
	}
    }
    if_p = ifl_find_name(S_interfaces, ifn);
    if (if_p == NULL) {
	/* interface doesn't exist */
	goto done;
    }
    link = if_link_status_update(if_p);
    if (link.valid) {
	/* make sure address information is up to date */
	if_link_update(if_p);
    }
    if (ifstate == NULL || ifstate->netboot) {
	/* don't propagate media status events for netboot interface */
	goto done;
    }
    if (if_p != NULL) {
	if_link_copy(ifstate->if_p, if_p);
    }
    if (G_IPConfiguration_verbose) {
	if (link.valid == FALSE) {
	    my_log(LOG_NOTICE, "%s link is unknown", ifn);
	}
	else {
	    my_log(LOG_NOTICE, "%s link %s",
		   ifn, link.active ? "ACTIVE" : "INACTIVE");
	}
    }
    if (if_is_wireless(ifstate->if_p)) {
	struct ether_addr	bssid;
	CFStringRef		ssid;

	ssid = S_copy_ssid_bssid(ifstate->ifname, &bssid);
	if (G_IPConfiguration_verbose) {
	    if (ssid != NULL) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("%s: SSID %@ BSSID %s"),
		      ifn, ssid, ether_ntoa(&bssid));
	    }
	    else {
		my_log(LOG_NOTICE, "%s: no SSID", ifn);
	    }
	}
	if (ssid != NULL) {
	    if (ifstate->ssid != NULL
		&& !CFEqual(ssid, ifstate->ssid)) {
		network_changed = (void *)1;
	    }
	    /* remember the last ssid */
	    IFState_set_ssid_bssid(ifstate, ssid, &bssid);
	    CFRelease(ssid);
	}
    }
    if (link.valid && link.active == FALSE) {
	struct timeval	tv;

	if (G_IPConfiguration_verbose) {
	    my_log(LOG_NOTICE,
		   "%s: scheduling link inactive timer for %d second(s)",
		   ifn, S_link_inactive_secs);
	}
	tv.tv_sec = S_link_inactive_secs;
	tv.tv_usec = 0;
	timer_set_relative(ifstate->timer, tv, link_timer_expired,
			   ifstate, NULL, NULL);
    }
    else {
	timer_cancel(ifstate->timer);
    }

    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	ServiceRef	service_p = dynarray_element(&ifstate->services, j);

	config_method_media(service_p, network_changed);
    }
    if ((link.valid == FALSE || link.active)
	&& dynarray_count(&ifstate->services_v6) != 0
	&& if_ift_type(ifstate->if_p) != IFT_STF) {
	/* start IPv6 Link Local */
	(void)inet6_linklocal_start(if_name(if_p));
    }
    service_list_event(&ifstate->services_v6, IFEventID_link_status_changed_e,
		       (void *)network_changed);
 done:
    my_CFRelease(&dict);
    my_CFRelease(&ifn_cf);
    return;
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
    IFStateRef   		ifstate;

    ifn_cf = IPv4ARPCollisionKeyParse(cache_key, &ip_addr, &hwaddr, &hwlen);
    if (ifn_cf == NULL || hwaddr == NULL) {
	goto done;
    }
    my_CFStringToCStringAndLength(ifn_cf, ifn, sizeof(ifn));
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
    service_list_event(&ifstate->services, IFEventID_arp_collision_e, &evdata);

 done:
    if (hwaddr != NULL) {
	free(hwaddr);
    }
    my_CFRelease(&ifn_cf);
    return;
}

static void
dhcp_preferences_changed(SCPreferencesRef prefs,
			 SCPreferencesNotification type,
			 void * info)
{
    int i;

    if ((type & kSCPreferencesNotificationApply) == 0) {
	return;
    }

    /* merge in the new requested parameters */
    S_add_dhcp_parameters(prefs);
    SCPreferencesSynchronize(prefs);

    for (i = 0; i < dynarray_count(&S_ifstate_list); i++) {
	IFStateRef	ifstate = dynarray_element(&S_ifstate_list, i);
	int		j;

	/* ask each service to renew immediately to pick up new options */
	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    ServiceRef	service_p = dynarray_element(&ifstate->services, j);

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
    CFIndex		i;
    boolean_t		iflist_changed = FALSE;
    boolean_t		name_changed = FALSE;
    boolean_t		order_changed = FALSE;
    CFStringRef		setup_global_ipv4_key = NULL;
    CFMutableArrayRef	state_changes = NULL;

    count = CFArrayGetCount(changes);
    if (count == 0) {
	goto done;
    }
    if (G_IPConfiguration_verbose) {
	SCLog(TRUE, LOG_NOTICE, 
	      CFSTR("Changes: %@ (%d)"), changes, count);
    }
    for (i = 0; i < count; i++) {
	CFStringRef	cache_key = CFArrayGetValueAtIndex(changes, i);

	if (CFEqual(cache_key, S_computer_name_key)
	    || CFEqual(cache_key, S_hostnames_key)) {
	    name_changed = TRUE;
	}
        else if (CFStringHasPrefix(cache_key, kSCDynamicStoreDomainSetup)) {
	    if (setup_global_ipv4_key == NULL) {
		setup_global_ipv4_key
		    = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
								 kSCDynamicStoreDomainSetup,
								 kSCEntNetIPv4);
	    }
	    if (CFEqual(cache_key, setup_global_ipv4_key)) {
		/* service order may have changed */
		order_changed = TRUE;
	    }
	    /* IPv4 configuration changed */
	    config_changed = TRUE;
	}
	else if (CFStringHasSuffix(cache_key, kSCCompInterface)) {
	    /* list of interfaces changed */
	    iflist_changed = TRUE;
	}
	else {
	    if (state_changes == NULL) {
		state_changes
		    = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
	    }
	    CFArrayAppendValue(state_changes, cache_key);
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
    /* look through remaining State: key changes */
    if (state_changes != NULL) {
	count = CFArrayGetCount(state_changes);
    }
    else {
	count = 0;
    }
    for (i = 0; i < count; i++) {
	CFStringRef	cache_key = CFArrayGetValueAtIndex(state_changes, i);

	if (CFStringHasSuffix(cache_key, kSCEntNetLink)) {
	    link_key_changed(session, cache_key);
	}
	else if (CFStringHasSuffix(cache_key, kSCEntNetAirPort)) {
	    ap_key_changed(session, cache_key);
	}
	else if (CFStringHasSuffix(cache_key, kSCEntNetRefreshConfiguration)) {
	    link_refresh(session, cache_key);
	}
	else if (CFStringHasSuffix(cache_key, kSCEntNetIPv6)) {
	    ipv6_interface_address_changed(session, cache_key);
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
	linklocal_set_needs_attention();
    }
 done:
    my_CFRelease(&setup_global_ipv4_key);
    my_CFRelease(&state_changes);
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
	IFStateRef	ifstate = dynarray_element(&S_ifstate_list, i);
	int		j;

	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    ServiceRef service_p = dynarray_element(&ifstate->services, j);
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
ServiceRemoveAddressConflict(ServiceRef service_p)
{
    if (service_p->user_rls != NULL) {
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
service_notify_user(ServiceRef service_p, CFArrayRef header,
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
    ServiceRemoveAddressConflict(service_p);
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

static void
service_report_conflict(ServiceRef service_p, CFStringRef ip_str)
{
    CFArrayRef		array = NULL;
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
    values[0] = CFSTR("CONFLICT_HEADER_BEFORE_IP");
    values[1] = ip_str;
    values[2] = CFSTR("CONFLICT_HEADER_AFTER_IP");
    array = CFArrayCreate(NULL, (const void **)values, 3,
			  &kCFTypeArrayCallBacks);
    service_notify_user(service_p, array, CFSTR("CONFLICT_MESSAGE"));
    CFRelease(array);
    return;
}

void
ServiceReportIPv4AddressConflict(ServiceRef service_p, struct in_addr addr)
{
    CFStringRef         ip_str = NULL;

    ip_str = my_CFStringCreateWithIPAddress(addr);
    service_report_conflict(service_p, ip_str);
    CFRelease(ip_str);
    return;
}

void
ServiceReportIPv6AddressConflict(ServiceRef service_p,
				 const struct in6_addr * addr_p)
{
    CFStringRef         ip_str = NULL;

    ip_str = my_CFStringCreateWithIPv6Address(addr_p);
    service_report_conflict(service_p, ip_str);
    CFRelease(ip_str);
    return;
}

void
ServiceRemoveIPv6AddressConflict(service_p)
{
}

#endif /* TARGET_OS_EMBEDDED */

/**
 ** Routines to read configuration and convert from CF types to
 ** simple types.
 **/
static boolean_t
S_get_plist_boolean_quiet(CFDictionaryRef plist, CFStringRef key,
			  boolean_t def)
{
    CFBooleanRef 	b;
    boolean_t		ret = def;

    b = isA_CFBoolean(CFDictionaryGetValue(plist, key));
    if (b) {
	ret = CFBooleanGetValue(b);
    }
    return (ret);
}

static boolean_t
S_get_plist_boolean(CFDictionaryRef plist, CFStringRef key,
		    boolean_t def)
{
    boolean_t	ret;
    ret = S_get_plist_boolean_quiet(plist, key, def);
    if (G_IPConfiguration_verbose) {
	SCLog(TRUE, LOG_NOTICE, 
	      CFSTR("%@ = %s"), key, ret == TRUE ? "true" : "false");
    }
    return (ret);
}

static int
S_get_plist_int_quiet(CFDictionaryRef plist, CFStringRef key,
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
    return (ret);
}

static int
S_get_plist_int(CFDictionaryRef plist, CFStringRef key,
		int def)
{
    int		ret;

    ret = S_get_plist_int_quiet(plist, key, def);
    if (G_IPConfiguration_verbose) {
	SCLog(TRUE, LOG_NOTICE, CFSTR("%@ = %d"), key, ret);
    }
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
    if (G_IPConfiguration_verbose) {
	SCLog(TRUE, LOG_NOTICE, 
	      CFSTR("%@ = %d.%06d"), key, ret.tv_sec,
	      ret.tv_usec);
    }
    return (ret);
}

static void *
S_get_number_array(CFArrayRef arr, int num_size, int * ret_count)
{
    void *	buf = NULL;
    int		count;
    int 	i;
    int 	real_count = 0;
    void *	scan;

    switch (num_size) {
    case 1:
    case 2:
    case 4:
	break;
    default:
	goto done;
    }
    count = CFArrayGetCount(arr);
    if (count == 0) {
	goto done;
    }
    buf = malloc(count * num_size);
    if (buf == NULL) {
	goto done;
    }
    for (i = 0, real_count = 0, scan = buf; i < count; i++) {
	CFNumberRef	n = isA_CFNumber(CFArrayGetValueAtIndex(arr, i));
	int		val;

	if (n == NULL
	    || CFNumberGetValue(n, kCFNumberIntType, &val) == FALSE) {
	    continue;
	}
	switch (num_size) {
	case 1:
	    *((uint8_t *)scan) = val;
	    break;
	case 2:
	    *((uint16_t *)scan) = val;
	    break;
	case 4:
	    *((uint32_t *)scan) = val;
	    break;
	default:
	    break;
	}
	real_count++;
	scan += num_size;
    }
 done:
    *ret_count = real_count;
    if (real_count == 0 && buf != NULL) {
	free(buf);
	buf = NULL;
    }
    return (buf);
}

static void *
S_get_plist_number_array(CFDictionaryRef plist, CFStringRef key,
			 int num_size, int * ret_count)
{
    CFArrayRef	a;

    a = isA_CFArray(CFDictionaryGetValue(plist, key));
    if (a == NULL) {
	return (NULL);
    }
    return (S_get_number_array(a, num_size, ret_count));
}

static uint8_t *
S_get_plist_uint8_array(CFDictionaryRef plist, CFStringRef key,
			int * ret_count)
{
    return (S_get_plist_number_array(plist, key, sizeof(uint8_t), ret_count));
}

static uint16_t *
S_get_plist_uint16_array(CFDictionaryRef plist, CFStringRef key,
			 int * ret_count)
{
    return (S_get_plist_number_array(plist, key, sizeof(uint16_t), ret_count));
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
applicationRequestedParametersCopy(SCPreferencesRef prefs)
{
    CFPropertyListRef data = NULL;
    CFMutableArrayRef array = NULL;

    data = SCPreferencesGetValue(prefs, kDHCPClientApplicationPref);
    if (isA_CFDictionary(data) == NULL) {
	goto done;
    }
    if (G_IPConfiguration_verbose) {
	SCLog(TRUE, LOG_NOTICE, 
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
    uint8_t *		dhcp_params = NULL;
    CFDictionaryRef 	info_dict;
    int			n_dhcp_params = 0;
    CFDictionaryRef	plist;

    if (S_bundle == NULL) {
	return;
    }
    info_dict = CFBundleGetInfoDictionary(S_bundle);
    if (info_dict == NULL) {
	return;
    }
    plist = CFDictionaryGetValue(info_dict, CFSTR("IPConfiguration"));
    if (isA_CFDictionary(plist) == NULL) {
	return;
    }
    G_IPConfiguration_verbose 
	= S_get_plist_boolean(plist, CFSTR("Verbose"), FALSE);
    G_must_broadcast 
	= S_get_plist_boolean(plist, CFSTR("MustBroadcast"), FALSE);
    G_max_retries = S_get_plist_int(plist, CFSTR("RetryCount"), 
				    MAX_RETRIES);
    G_gather_secs = S_get_plist_int(plist, CFSTR("GatherTimeSeconds"), 
				    GATHER_SECS);
    S_link_inactive_secs 
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
    S_arp_resolve_retry 
	= S_get_plist_timeval(plist, CFSTR("ARPResolveRetryTimeSeconds"),
			      S_arp_resolve_retry);
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
	= S_get_plist_uint8_array(plist,
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
    S_configure_ipv6 = S_get_plist_boolean(plist,
					   CFSTR("ConfigureIPv6"),
					   TRUE);
    if (S_configure_ipv6) {
	uint16_t *	dhcpv6_options;
	int		dhcpv6_options_count;

	G_dhcpv6_enabled = S_get_plist_boolean(plist,
					       CFSTR("DHCPv6Enabled"),
					       DHCPv6_ENABLED);
	dhcpv6_options = S_get_plist_uint16_array(plist,
						  kDHCPv6RequestedOptions,
						  &dhcpv6_options_count);
	DHCPv6ClientSetRequestedOptions(dhcpv6_options,
					dhcpv6_options_count);
	G_dhcpv6_stateful_enabled = S_get_plist_boolean(plist,
							CFSTR("DHCPv6StatefulEnabled"),
							DHCPv6_STATEFUL_ENABLED);
	S_ipv6_map_automatic_to_linklocal
	    = S_get_plist_boolean(plist,
				  CFSTR("IPv6MapAutomaticToLinkLocal"),
				  IPV6_MAP_AUTOMATIC_TO_LINKLOCAL);
	G_dhcp_duid_type = S_get_plist_int(plist, 
					   CFSTR("DHCPDUIDType"),
					   kDHCPDUIDTypeLLT);
	switch (G_dhcp_duid_type) {
	case kDHCPDUIDTypeLLT:
	case kDHCPDUIDTypeLL:
	    /* supported */
	    break;
	default:
	    /* unsupported, use default (LLT) */
	    G_dhcp_duid_type = kDHCPDUIDTypeLLT;
	    break;
	}
    }
    return;
}

static void
S_add_dhcp_parameters(SCPreferencesRef prefs)
{
    uint8_t *	dhcp_params = NULL;
    int		n_dhcp_params = 0;
    CFArrayRef	rp = applicationRequestedParametersCopy(prefs);

    if (rp != NULL) {
	dhcp_params = S_get_number_array(rp, sizeof(*dhcp_params),
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
    SCPreferencesRef 		prefs = NULL;

    if (server_active()) {
	my_log(LOG_NOTICE, "ipconfig server already active");
	fprintf(stderr, "ipconfig server already active\n");
	return;
    }

    S_set_globals();
    prefs = SCPreferencesCreate(NULL, CFSTR("IPConfiguration.DHCPClient"),
				kDHCPClientPreferencesID);
    if (prefs == NULL) {
	my_log(LOG_ERR, 
	       "IPConfiguration: SCPreferencesCreate failed: %s",
	       SCErrorString(SCError()));
	return;
    }
    if (SCPreferencesSetCallback(prefs,
				 dhcp_preferences_changed,
				 NULL) == FALSE
	|| SCPreferencesScheduleWithRunLoop(prefs,
					    CFRunLoopGetCurrent(),
					    kCFRunLoopDefaultMode) == FALSE) {
	my_log(LOG_ERR, "IPConfigurationSCPreferencesSetCallback failed: %s",
	       SCErrorString(SCError()));
	my_CFRelease(&prefs);
	return;
    }
    S_add_dhcp_parameters(prefs);
    SCPreferencesSynchronize(prefs);

    S_scd_session = SCDynamicStoreCreate(NULL, 
					 CFSTR("IPConfiguration"),
					 handle_change, NULL);
    if (S_scd_session == NULL) {
	S_scd_session = NULL;
	my_log(LOG_ERR, "SCDynamicStoreCreate failed: %s", 
	       SCErrorString(SCError()));
    }

    G_bootp_session = bootp_session_init(G_client_port);
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
    arp_values.resolve_interval = &S_arp_resolve_retry;
    G_arp_session = arp_session_init(S_is_our_hardware_address,
				     &arp_values);
    if (G_arp_session == NULL) {
	my_log(LOG_DEBUG, "arp_session_init() failed");
	return;
    }

    if (G_IPConfiguration_verbose) {
	S_dhcp_packet_log = logfile_fopen(IPCONFIGURATION_BOOTP_LOG);
	bootp_session_set_debug(G_bootp_session, S_dhcp_packet_log);
	if (G_dhcpv6_enabled) {
	    S_dhcpv6_packet_log
		= logfile_fopen(IPCONFIGURATION_DHCPV6_LOG);
	    DHCPv6SocketSetLogFile(S_dhcpv6_packet_log);
	}
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
	IFStateList_all_services_event(&S_ifstate_list,
				       IFEventID_power_off_e, NULL);
    }
    CFRunLoopSourceSignal(stopRls);
}
