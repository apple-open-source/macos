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
#include "ipconfigd_globals.h"
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCDPlugin.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <TargetConditionals.h>
#include <Availability.h>
#include <os/state_private.h>
#include <os/variant_private.h>
#if TARGET_OS_OSX
#include <CoreFoundation/CFUserNotification.h>
#include <CoreFoundation/CFUserNotificationPriv.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#endif /* TARGET_OS_OSX */

#include "rfc_options.h"
#include "dhcp_options.h"
#include "interfaces.h"
#include "util.h"
#include "IPv4ClasslessRoute.h"

#include "dhcplib.h"
#include "ioregpath.h"

#include "ipconfig_types.h"
#include "ipconfigd.h"
#include "server.h"
#include "timer.h"

#include "ipconfigd_threads.h"
#include "FDSet.h"

#include "symbol_scope.h"
#include "cfutil.h"
#include "sysconfig.h"
#include "ifutil.h"
#include "rtutil.h"
#include "DHCPv6Client.h"
#include "DHCPv6Socket.h"
#include "IPConfigurationServiceInternal.h"
#include "IPConfigurationControlPrefs.h"
#include "CGA.h"
#include "ICMPv6Socket.h"
#include "DHCPDUIDIAID.h"
#include "report_symptoms.h"
#include "bootp_transmit.h"
#include "wireless.h"

#define IPCONFIGURATION_SUBSYSTEM	"com.apple.SystemConfiguration.IPConfiguration"

#define RIFLAGS_IADDR_VALID		((uint32_t)0x1)
#define RIFLAGS_HWADDR_VALID		((uint32_t)0x2)
#define RIFLAGS_ARP_VERIFIED		((uint32_t)0x4)
#define RIFLAGS_RESOLVE_IN_PROGRESS	((uint32_t)0x8)
#define RIFLAGS_RESOLVE_TIMED_OUT	((uint32_t)0x10)
#define RIFLAGS_ALL_VALID		(RIFLAGS_IADDR_VALID		\
					 | RIFLAGS_HWADDR_VALID		\
					 | RIFLAGS_ARP_VERIFIED)

#define kARPResolvedIPAddress		CFSTR("ARPResolvedIPAddress")
#define kARPResolvedHardwareAddress	CFSTR("ARPResolvedHardwareAddress")

#define IPV6_LL_MAX_COLLISION_COUNT	3

typedef struct {
    uint32_t			flags;
    struct in_addr		iaddr;
    uint8_t			hwaddr[MAX_LINK_ADDR_LEN];
} router_info_t;

STATIC void service_router_clear_arp_verified(ServiceRef service_p);
STATIC void service_router_set_resolve_in_progress(ServiceRef service_p);
STATIC void service_router_clear_resolve_in_progress(ServiceRef service_p);
STATIC boolean_t service_router_resolve_in_progress(ServiceRef service_p);
STATIC void service_router_set_resolve_timed_out(ServiceRef service_p);
STATIC void service_router_clear_resolve_timed_out(ServiceRef service_p);
STATIC boolean_t service_router_resolve_timed_out(ServiceRef service_p);


typedef struct IFState * IFStateRef;

typedef struct {
    struct in_addr	addr;
    struct in_addr	mask;
    struct in_addr	dest;
} ip_addr_mask_t;

typedef struct {
    ip_addr_mask_t	requested_ip;
    inet_addrinfo_t	info;
    router_info_t	router;
    absolute_time_t	ip_assigned_time;
    absolute_time_t	ip_conflict_time;
    int			ip_conflict_count;
} ServiceIPv4, * ServiceIPv4Ref;

typedef struct {
    struct in6_addr		addr;
    int				prefix_length;
} inet6_addr_prefix_t;

typedef struct {
    inet6_addr_prefix_t		requested_ip;
    boolean_t			enable_clat46;
    boolean_t			disable_dhcpv6;
} ServiceIPv6, * ServiceIPv6Ref;

struct ServiceInfo {
    CFStringRef			serviceID;
    CFStringRef			apn_name;
    IFStateRef			ifstate;
    ipconfig_method_t		method;
    ipconfig_status_t		status;
    boolean_t			is_dynamic;
    boolean_t			no_publish;
    boolean_t			ready;
    CFStringRef			parent_serviceID;
    CFStringRef			child_serviceID;
    dispatch_source_t		pid_source;
    boolean_t			busy;
    void * 			private;
#if TARGET_OS_OSX
    CFUserNotificationRef	user_notification;
    CFRunLoopSourceRef		user_rls;
#endif /* TARGET_OS_OSX */
    union {
	ServiceIPv4		v4;
	ServiceIPv6		v6;
    } u;
};

typedef struct {
    boolean_t			needs_attention;
    boolean_t			requested;
} ActiveDuringSleepInfo, * ActiveDuringSleepInfoRef;

typedef struct {
    boolean_t			interface_disabled;
    boolean_t			prefs_set;
    boolean_t			prefs_requested;
} DisableUntilNeededInfo, * DisableUntilNeededInfoRef;

typedef CF_ENUM(uint32_t, IFStateFlags) {
    kIFStateFlagsStartupReady			= 0x00000001,
    kIFStateFlagsBusy				= 0x00000002,
    kIFStateFlagsServicesReady			= 0x00000004,
    kIFStateFlagsFailureSymptomReported		= 0x00000008,
    kIFStateFlagsNetBoot			= 0x00000010,
    kIFStateFlagsLinkTimerSuppressed		= 0x00000020,
    kIFStateFlagsIPv4PublishNeedsAttention	= 0x00000040,
    kIFStateFlagsIPv4AddressesScrubbed		= 0x00000080,

    kIFStateFlagsDisableCGA			= 0x00010000,
    kIFStateFlagsDisablePerformNUD		= 0x00020000,
    kIFStateFlagsDisableDAD			= 0x00040000,
    kIFStateFlagsPLATDiscoveryComplete		= 0x00080000,
    kIFStateFlagsNAT64PrefixAvailable		= 0x00100000,
    kIFStateFlagsCLAT46Available		= 0x00200000,
    kIFStateFlagsCLAT46Active			= 0x00400000,
};

struct IFState {
    IFStateFlags		flags;
    interface_t *		if_p;
    CFStringRef			ifname;
    dynarray_t			services;
    dynarray_t			services_v6;
    ServiceRef			linklocal_service_p;
    WiFiInfoRef			wifi_info_p;
    timer_callout_t *		timer;
    struct in_addr		v4_link_local;
    uint32_t			wake_generation;
    struct in6_addr		ipv6_linklocal;
    CFMutableArrayRef		neighbor_advert_list;
    ActiveDuringSleepInfo	active_during_sleep;
    DisableUntilNeededInfo	disable_until_needed;
    unsigned int		rank;
    uint8_t			v6_ll_collision_count;
};

STATIC Boolean
FlagsAreSet(uint32_t flags, uint32_t flags_to_check)
{
    return ((flags & flags_to_check) != 0);
}

STATIC Boolean
IFStateFlagsAreSet(IFStateRef ifstate, IFStateFlags flags)
{
    return (FlagsAreSet(ifstate->flags, flags));
}

STATIC void
IFStateFlagsSet(IFStateRef ifstate, IFStateFlags flags)
{
    ifstate->flags |= flags;
}

STATIC void
IFStateFlagsClear(IFStateRef ifstate, IFStateFlags flags)
{
    ifstate->flags &= ~flags;
}

STATIC void
IFStateFlagsSetOrClear(IFStateRef ifstate, IFStateFlags flags,
		       boolean_t set)
{
    if (set) {
	IFStateFlagsSet(ifstate, flags);
    }
    else {
	IFStateFlagsClear(ifstate, flags);
    }
}

typedef dynarray_t	IFStateList_t;

typedef void (^ServiceInitHandler)(ServiceRef service);

#define IS_IPV4		TRUE
#define IS_IPV6		FALSE

#ifndef kSCEntNetRefreshConfiguration
#define kSCEntNetRefreshConfiguration	CFSTR("RefreshConfiguration")
#endif /* kSCEntNetRefreshConfiguration */

#ifndef kSCEntNetIPv4ARPCollision
#define kSCEntNetIPv4ARPCollision	CFSTR("IPv4ARPCollision")
#endif /* kSCEntNetIPv4ARPCollision */

#ifndef kSCEntNetNAT64
#define kSCEntNetNAT64			CFSTR("NAT64")
#endif /* kSCEntNetNAT64 */

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

#ifndef kSCPropNetIPv6EnableCGA
/* CFNumber (0,1), default 1 */
static const CFStringRef kSCPropNetIPv6EnableCGA = CFSTR("EnableCGA");
#define kSCPropNetIPv6EnableCGA		kSCPropNetIPv6EnableCGA
#endif /* kSCPropNetIPv66to4Relay */

#ifndef kSCPropNetIPv6LinkLocalAddress
static const CFStringRef kSCPropNetIPv6LinkLocalAddress = CFSTR("LinkLocalAddress");
#define kSCPropNetIPv6LinkLocalAddress	kSCPropNetIPv6LinkLocalAddress
#endif /* kSCPropNetIPv6LinkLocalAddress */

#ifndef kSCPropNetIPv6PerformPLATDiscovery
static const CFStringRef kSCPropNetIPv6PerformPLATDiscovery = CFSTR("PerformPLATDiscovery");
#define kSCPropNetIPv6PerformPLATDiscovery	kSCPropNetIPv6PerformPLATDiscovery
#endif /* kSCPropNetIPv6PerformPLATDiscovery */

#ifndef kSCPropNetNAT64PLATDiscoveryCompletionTime
static const CFStringRef kSCPropNetNAT64PLATDiscoveryCompletionTime = CFSTR("PLATDiscoveryCompletionTime");
#endif /* kSCPropNetNAT64PLATDiscoveryCompletionTime */

#ifndef kSCPropNetIPv4CLAT46
static const CFStringRef kSCPropNetIPv4CLAT46 = CFSTR("CLAT46");
#define kSCPropNetIPv4CLAT46	kSCPropNetIPv4CLAT46
#endif /* kSCPropNetIPv4CLAT46 */

#ifndef kSCEntNetInterfaceActiveDuringSleepRequested
#define kSCEntNetInterfaceActiveDuringSleepRequested	CFSTR("ActiveDuringSleepRequested")
#endif /* kSCEntNetInterfaceActiveDuringSleepRequested */

#ifndef kSCEntNetInterfaceActiveDuringSleepSupported
#define kSCEntNetInterfaceActiveDuringSleepSupported	CFSTR("ActiveDuringSleepSupported")
#endif /* kSCEntNetInterfaceActiveDuringSleepSupported */

#ifndef kSCPropNetInterfaceDisableUntilNeeded
/* number (0,1) */
#define kSCPropNetInterfaceDisableUntilNeeded	CFSTR("DisableUntilNeeded")
#endif /* kSCPropNetInterfaceDisableUntilNeeded */

#ifndef kSCEntNetIPv6RouterExpired
#define kSCEntNetIPv6RouterExpired		CFSTR("IPv6RouterExpired")
#endif /* kSCEntNetIPv6RouterExpired */

#ifndef kSCEntNetInterfaceIPConfigurationBusy
#define kSCEntNetInterfaceIPConfigurationBusy	CFSTR("IPConfigurationBusy")
#endif /* kSCEntNetInterfaceIPConfigurationBusy */

#define k_DisableUntilNeeded		CFSTR("_DisableUntilNeeded") /* bool */

#define kDHCPClientPreferencesID	CFSTR("DHCPClient.plist")
#define kDHCPClientApplicationPref	CFSTR("Application")
#define kDHCPRequestedParameterList	CFSTR("DHCPRequestedParameterList")

#define kDHCPv6RequestedOptions		CFSTR("DHCPv6RequestedOptions")

/* default values */
#define MAX_RETRIES				9
#define INITIAL_WAIT_SECS			1
#define MAX_WAIT_SECS				8
#define GATHER_SECS				1
#define LINK_INACTIVE_WAIT_SECS			0.1
#define DHCP_INIT_REBOOT_RETRY_COUNT		2
#define DHCP_SELECT_RETRY_COUNT			3
#define DHCP_ROUTER_ARP_AT_RETRY_COUNT		3
#define DHCP_ALLOCATE_LINKLOCAL_AT_RETRY_COUNT	4
#define DHCP_GENERATE_FAILURE_SYMPTOM_AT_RETRY_COUNT 6
#define DHCP_FAILURE_CONFIGURES_LINKLOCAL	TRUE
#define DHCP_SUCCESS_DECONFIGURES_LINKLOCAL	TRUE
#define DHCP_LOCAL_HOSTNAME_LENGTH_MAX		15
#define DISCOVER_ROUTER_MAC_ADDRESS_SECS	60
#define DEFEND_IP_ADDRESS_INTERVAL_SECS		10
#define DEFEND_IP_ADDRESS_COUNT			5
#define DHCP_LEASE_WRITE_T1_THRESHOLD_SECS	3600
#define MANUAL_CONFLICT_RETRY_INTERVAL_SECS	300		

#define USER_ERROR			1
#define UNEXPECTED_ERROR 		2
#define TIMEOUT_ERROR			3

/* global variables */
PRIVATE_EXTERN uint16_t 	G_client_port = IPPORT_BOOTPC;
PRIVATE_EXTERN boolean_t 	G_dhcp_accepts_bootp = FALSE;
PRIVATE_EXTERN boolean_t	G_dhcp_failure_configures_linklocal 
				    = DHCP_FAILURE_CONFIGURES_LINKLOCAL;
PRIVATE_EXTERN boolean_t	G_dhcp_success_deconfigures_linklocal 
				    = DHCP_SUCCESS_DECONFIGURES_LINKLOCAL;
PRIVATE_EXTERN int		G_dhcp_allocate_linklocal_at_retry_count 
				    = DHCP_ALLOCATE_LINKLOCAL_AT_RETRY_COUNT;
PRIVATE_EXTERN int		G_dhcp_generate_failure_symptom_at_retry_count
				    = DHCP_GENERATE_FAILURE_SYMPTOM_AT_RETRY_COUNT;
PRIVATE_EXTERN int		G_dhcp_router_arp_at_retry_count 
				    = DHCP_ROUTER_ARP_AT_RETRY_COUNT;
PRIVATE_EXTERN int		G_dhcp_init_reboot_retry_count 
				    = DHCP_INIT_REBOOT_RETRY_COUNT;
PRIVATE_EXTERN int		G_dhcp_select_retry_count 
				    = DHCP_SELECT_RETRY_COUNT;
PRIVATE_EXTERN int		G_dhcp_lease_write_t1_threshold_secs
				    = DHCP_LEASE_WRITE_T1_THRESHOLD_SECS;
PRIVATE_EXTERN uint16_t 	G_server_port = IPPORT_BOOTPS;
PRIVATE_EXTERN int		G_manual_conflict_retry_interval_secs
				    = MANUAL_CONFLICT_RETRY_INTERVAL_SECS;

PRIVATE_EXTERN boolean_t	G_is_netboot = FALSE;
PRIVATE_EXTERN IPConfigurationInterfaceTypes
				G_awd_interface_types = kIPConfigurationInterfaceTypesCellular;
;

/* 
 * Static: S_link_inactive_secs
 * Purpose:
 *   Time to wait after the link goes inactive before unpublishing 
 *   the interface state information
 */
static CFTimeInterval		S_link_inactive_secs = LINK_INACTIVE_WAIT_SECS;

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

boolean_t 			G_must_broadcast;
Boolean				G_IPConfiguration_verbose;
boolean_t			G_router_arp = TRUE;
int				G_router_arp_wifi_lease_start_threshold_secs = (3600 * 24); /* 1 day */
boolean_t			G_discover_and_publish_router_mac_address = TRUE;

#define DHCPv6_ENABLED		TRUE
#define DHCPv6_STATEFUL_ENABLED	TRUE

boolean_t			G_dhcpv6_enabled = DHCPv6_ENABLED;
boolean_t			G_dhcpv6_stateful_enabled = DHCPv6_STATEFUL_ENABLED;
DHCPDUIDType			G_dhcp_duid_type;

const unsigned char		G_rfc_magic[4] = RFC_OPTIONS_MAGIC;
const struct in_addr		G_ip_broadcast = { INADDR_BROADCAST };
const struct in_addr		G_ip_zeroes = { 0 };

#define MIN_SHORT_WAKE_INTERVAL_SECS	60
PRIVATE_EXTERN int		G_min_short_wake_interval_secs = MIN_SHORT_WAKE_INTERVAL_SECS;
#define MIN_WAKE_INTERVAL_SECS	(60 * 15)
PRIVATE_EXTERN int		G_min_wake_interval_secs = MIN_WAKE_INTERVAL_SECS;
#define WAKE_SKEW_SECS		30
PRIVATE_EXTERN int		G_wake_skew_secs = WAKE_SKEW_SECS;

/* local variables */
static interface_list_t *	S_interfaces;
static CFBundleRef		S_bundle;
static boolean_t		S_linklocal_needs_attention;
static boolean_t		S_ipv4_publish_needs_attention;
static IFStateList_t		S_ifstate_list;
static io_connect_t 		S_power_connection;
static SCDynamicStoreRef	S_scd_session;
static CFStringRef		S_setup_service_prefix;
static CFStringRef		S_state_interface_prefix;
static char * 			S_computer_name;
static CFStringRef		S_computer_name_key;
static CFStringRef		S_hostnames_key;
static int			S_arp_probe_count = ARP_PROBE_COUNT;
static int			S_arp_gratuitous_count = ARP_GRATUITOUS_COUNT;
static CFTimeInterval		S_arp_retry = ARP_RETRY_SECS;
static int			S_arp_detect_count = ARP_DETECT_COUNT;
static CFTimeInterval		S_arp_detect_retry = ARP_DETECT_RETRY_SECS;
static int			S_discover_router_mac_address_secs
					= DISCOVER_ROUTER_MAC_ADDRESS_SECS;

static int			S_arp_conflict_retry = ARP_CONFLICT_RETRY_COUNT;
static CFTimeInterval		S_arp_conflict_delay = ARP_CONFLICT_RETRY_DELAY_SECS;
static int			S_defend_ip_address_interval_secs 
				    = DEFEND_IP_ADDRESS_INTERVAL_SECS;
static int			S_defend_ip_address_count
				    = DEFEND_IP_ADDRESS_COUNT;


static int S_dhcp_local_hostname_length_max = DHCP_LOCAL_HOSTNAME_LENGTH_MAX;

static struct in_addr		S_netboot_ip;
static struct in_addr		S_netboot_server_ip;
static char			S_netboot_ifname[IFNAMSIZ + 1];

static boolean_t		S_awake = TRUE;
#if TARGET_OS_OSX
static boolean_t		S_use_maintenance_wake = TRUE;
static boolean_t		S_wake_event_sent;
static boolean_t		S_hide_bssid;
static boolean_t		S_hide_bssid_was_set;
#endif /* TARGET_OS_OSX */
static uint32_t			S_wake_generation;
static absolute_time_t		S_wake_time;

static boolean_t		S_configure_ipv6 = TRUE;

STATIC boolean_t		S_active_during_sleep_needs_attention;
STATIC boolean_t		S_disable_until_needed_needs_attention;
STATIC boolean_t		S_disable_unneeded_interfaces = TRUE;

static boolean_t		S_cellular_clat46_autoenable;
static boolean_t		S_ipv6_linklocal_modifier_expires;

#define PROP_SERVICEID		CFSTR("ServiceID")

/*
 * forward declarations
 */
STATIC void
schedule_async_work(void);

static void
S_add_dhcp_parameters(SCPreferencesRef prefs);

static void
configuration_changed(SCDynamicStoreRef session);

static ipconfig_status_t
config_method_event(ServiceRef service_p, IFEventID_t event, void * data);

static ipconfig_status_t
config_method_start(ServiceRef service_p, ipconfig_method_info_t info);


static ipconfig_status_t
config_method_change(ServiceRef service_p,
		     ipconfig_method_info_t info,
		     boolean_t * needs_stop);


static ipconfig_status_t
config_method_stop(ServiceRef service_p);

static ipconfig_status_t
config_method_media(ServiceRef service_p, link_event_data_t link_event);

static ipconfig_status_t
config_method_bssid_changed(ServiceRef service_p);

static ipconfig_status_t
config_method_renew(ServiceRef service_p, link_event_data_t link_event);

static void
service_publish_clear(ServiceRef service_p, ipconfig_status_t status);

static boolean_t
all_services_ready();

static void
linklocal_elect(CFArrayRef service_order);

static CFArrayRef
S_copy_service_order(SCDynamicStoreRef session);

static __inline__ IFStateRef
service_ifstate(ServiceRef service_p)
{
    return (service_p->ifstate);
}

static __inline__ void
service_set_apn_name(ServiceRef service_p, CFStringRef apn_name)
{
    if (apn_name != NULL) {
	CFRetain(apn_name);
    }
    if (service_p->apn_name != NULL) {
	CFRelease(service_p->apn_name);
    }
    service_p->apn_name = apn_name;
    return;
}

static boolean_t
S_get_plist_boolean_quiet(CFDictionaryRef plist, CFStringRef key,
			  boolean_t def);
static int
S_get_plist_int_quiet(CFDictionaryRef plist, CFStringRef key,
		      int def);

typedef unsigned int Rank;

#define RANK_HIGHEST	(0)
#define RANK_LOWEST	(1024 * 1024)
#define RANK_NONE	(RANK_LOWEST + 1)

static IFStateRef
IFStateList_ifstate_with_name(IFStateList_t * list, const char * ifname,
			      int * where);

static IFStateRef
IFStateListGetIFState(IFStateList_t * list, CFStringRef ifname,
		      int * where);
static void
IFStateFreeService(IFStateRef ifstate, ServiceRef service_p);

static ServiceRef
IFState_service_with_ip(IFStateRef ifstate, struct in_addr iaddr);

static void
IFState_set_wifi_info(IFStateRef ifstate, WiFiInfoRef info_p);

STATIC void
IFStateSetActiveDuringSleepRequested(IFStateRef ifstate, boolean_t requested);

STATIC void
IFStateProcessActiveDuringSleep(IFStateRef ifstate);

STATIC boolean_t
IFStateGetDisableUntilNeededRequested(IFStateRef ifstate);

STATIC void
IFStateSetDisableUntilNeededRequested(IFStateRef ifstate, CFBooleanRef req);

STATIC boolean_t
IFState_attach_IPv6(IFStateRef ifstate, boolean_t set_iff_up);

STATIC void
IFState_detach_IPv4(IFStateRef ifstate);

STATIC void
IFState_detach_IPv6(IFStateRef ifstate);

STATIC const struct in6_addr *
IFStateIPv6LinkLocalAddress(IFStateRef ifstate);

STATIC void
IFStateSetIPv6LinkLocalAddress(IFStateRef ifstate,
			       const struct in6_addr * addr);

static void
linklocal_start(ServiceRef parent_service_p, boolean_t allocate);

static WiFiInfoRef
S_copy_wifi_info(CFStringRef ifname);

static int
S_remove_ip_address(const char * ifname, struct in_addr this_ip);

STATIC ipconfig_status_t
S_remove_service_with_id_str(const char * ifname, CFStringRef serviceID);

static ipconfig_status_t
method_info_from_dict(CFDictionaryRef dict,
		      ipconfig_method_info_t method_info);
static ipconfig_status_t
method_info_from_ipv6_dict(CFDictionaryRef dict,
			   ipconfig_method_info_t method_info);

STATIC CFDictionaryRef
ServiceIPv4CopyMergedDNSAndCaptive(ServiceRef service_p,
				   dhcp_info_t * info_p,
				   CFDictionaryRef *capport_dict_p);
STATIC CFDictionaryRef
ServiceIPv6CopyMergedDNSAndCaptive(ServiceRef service_p,
				   ipv6_info_t * info_v6_p,
				   CFDictionaryRef * ret_capport_dict);
STATIC Rank
ServiceGetRank(ServiceRef service_p, CFArrayRef service_order);

STATIC CFDictionaryRef
ServiceCopySummary(ServiceRef service_p);

STATIC void
ServiceSetIPv4PublishNeedsAttention(ServiceRef service_p);

STATIC void
ServiceSetIsPublished(ServiceRef service_p);

STATIC void
process_link_timer_expired(IFStateRef ifstate);

static void
service_list_event(dynarray_t * services_p, IFEventID_t event, void * data);

static ServiceRef
service_list_first_routable_service(dynarray_t * list);

STATIC boolean_t
service_list_check_busy(dynarray_t * services_p);

PRIVATE_EXTERN const char *
ipconfig_method_string(ipconfig_method_t m)
{
    const char * str;

    switch (m) {
    case ipconfig_method_none_e:
	str = "NONE";
	break;
    case ipconfig_method_none_v4_e:
	str = "NONE-V4";
	break;
    case ipconfig_method_none_v6_e:
	str = "NONE-V6";
	break;
    case ipconfig_method_manual_e:
	str = "MANUAL";
	break;
    case ipconfig_method_bootp_e:
	str = "BOOTP";
	break;
    case ipconfig_method_dhcp_e:
	str = "DHCP";
	break;
    case ipconfig_method_inform_e:
	str = "INFORM";
	break;
    case ipconfig_method_linklocal_e:
	str = "LINKLOCAL";
	break;
    case ipconfig_method_failover_e:
	str = "FAILOVER";
	break;
    case ipconfig_method_manual_v6_e:
	str = "MANUAL-V6";
	break;
    case ipconfig_method_automatic_v6_e:
	str = "AUTOMATIC-V6";
	break;
    case ipconfig_method_rtadv_e:
	str = "RTADV";
	break;
    case ipconfig_method_stf_e:
	str = "6TO4";
	break;
    case ipconfig_method_linklocal_v6_e:
	str = "LINKLOCAL-V6";
	break;
    case ipconfig_method_dhcpv6_pd_e:
	str = "DHCPV6-PD";
	break;
    }
    return (str);
}

STATIC boolean_t
ipconfig_method_routable(ipconfig_method_t m)
{
    boolean_t	routable;

    switch (m) {
    case ipconfig_method_bootp_e:
    case ipconfig_method_dhcp_e:
    case ipconfig_method_inform_e:
    case ipconfig_method_manual_e:
    case ipconfig_method_manual_v6_e:
    case ipconfig_method_automatic_v6_e:
    case ipconfig_method_rtadv_e:
	routable = TRUE;
	break;
    default:
	routable = FALSE;
	break;
    }
    return (routable);
}

STATIC Rank
service_list_get_rank(dynarray_t * services_p, CFArrayRef service_order,
		      boolean_t * services_ready_p);

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

/* State:/Network/Interface/en4/BonjourSleepProxyOPTRecord */
#define kBSPOPTRecord			CFSTR("BonjourSleepProxyOPTRecord")
#define kBSPOwnerOPTRecord		CFSTR("OwnerOPTRecord")

/* State:/Network/Interface/en4/BonjourSleepProxyAddress */
#define kBSPAddress			CFSTR("BonjourSleepProxyAddress")
#define kBSPMACAddress			kSCPropMACAddress
#define kBSPIPAddress			CFSTR("IPAddress")
#define kBSPRegisteredAddresses		CFSTR("RegisteredAddresses")	

STATIC CFStringRef
S_copy_bsp_address_key(CFStringRef ifname)
{
    CFStringRef key;
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							ifname,
							kBSPAddress);
    return (key);
}

STATIC CFStringRef
S_copy_bsp_opt_key(CFStringRef ifname)
{
    CFStringRef	key;
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							ifname,
							kBSPOPTRecord);
    return (key);
}

/*
 * Function: S_copy_sleep_proxy_info
 * Purpose:
 *   Copy the Bonjour Sleep Proxy Address information.
 */
STATIC CFDictionaryRef
S_copy_sleep_proxy_info(SCDynamicStoreRef store, CFStringRef ifname)
{
    CFDictionaryRef		info;
    CFStringRef			key;

    key = S_copy_bsp_address_key(ifname);
    info = SCDynamicStoreCopyValue(store, key);
    CFRelease(key);
    if (isA_CFDictionary(info) == NULL) {
	my_CFRelease(&info);
    }
    return (info);
}

STATIC boolean_t
S_is_sleep_proxy_conflict(SCDynamicStoreRef session,
			  CFStringRef ifname,
			  void * hwaddr, int hw_len,
			  struct in_addr * ret_sleep_proxy_ip,
			  CFDataRef * ret_opt_record)
{
    CFDictionaryRef	addr_info = NULL;
    CFStringRef		addr_key;
    CFDictionaryRef	info;
    boolean_t		is_sleep_proxy = FALSE;
#define N_BSP_KEYS 	2
    const void * 	keys[N_BSP_KEYS];
    CFArrayRef		key_list;
    CFDictionaryRef	opt_info = NULL;
    CFStringRef		opt_key;

    ret_sleep_proxy_ip->s_addr = 0;
    *ret_opt_record = NULL;
    keys[0] = addr_key = S_copy_bsp_address_key(ifname);
    keys[1] = opt_key = S_copy_bsp_opt_key(ifname);
    key_list = CFArrayCreate(NULL, keys, N_BSP_KEYS, &kCFTypeArrayCallBacks);
    info = SCDynamicStoreCopyMultiple(session, key_list, NULL);
    CFRelease(key_list);
    if (info != NULL) {
	addr_info = isA_CFDictionary(CFDictionaryGetValue(info, addr_key));
	opt_info = isA_CFDictionary(CFDictionaryGetValue(info, opt_key));
    }
    if (addr_info != NULL) {
	CFStringRef 	sp_mac_cf;

	sp_mac_cf = CFDictionaryGetValue(addr_info, kBSPMACAddress);
	if (isA_CFString(sp_mac_cf) != NULL) {
	    int		len;
	    void *	sp_mac;

	    sp_mac = bytesFromColonHexString(sp_mac_cf, &len);
	    if (sp_mac != NULL) {
		if (len == hw_len) {
		    if (bcmp(sp_mac, hwaddr, len) == 0) {
			is_sleep_proxy = TRUE;
		    }
		}
		free(sp_mac);
	    }
	}
    }
    if (is_sleep_proxy) {
	if (opt_info == NULL) {
	    my_log(LOG_NOTICE, "%@: BonjourSleepProxyOPTRecord is missing",
		   ifname);
	}
	else {
	    CFDataRef	opt_record;
	    CFStringRef	ip_addr;

	    ip_addr = CFDictionaryGetValue(addr_info, kBSPIPAddress);
	    if (my_CFStringToIPAddress(ip_addr, ret_sleep_proxy_ip)) {
		opt_record = CFDictionaryGetValue(opt_info, kBSPOwnerOPTRecord);
		if (isA_CFData(opt_record) != NULL) {
		    *ret_opt_record = CFRetain(opt_record);
		}
	    }
	}
    }
    my_CFRelease(&info);
    CFRelease(addr_key);
    CFRelease(opt_key);
    return (is_sleep_proxy);
}

/*
 * Function: S_process_neighbor_adverts
 * Purpose:
 *   Process the list of IPv6 addresses we registered with a sleep proxy.
 *   For each address, check whether it is in the list of addresses
 *   currently assigned to the interface.  If it is, then send a
 *   Neighbor Advertisement packet.
 *
 *   The list of IPv6 addresses is copied at wake, and tossed at sleep.
 *   If the list is more than 60 seconds old, also toss it to avoid needing
 *   to traverse it if we happen to end up with an address in there that's
 *   never going to get assigned (e.g. temporary address, or we switched
 *   networks).
 */
STATIC void
S_process_neighbor_adverts(IFStateRef ifstate, inet6_addrlist_t * addr_list_p)
{
    CFIndex		count;
    absolute_time_t	current_time;
    int			i;
    interface_t *	if_p;
    int			sockfd = -1;

    if (ifstate->neighbor_advert_list == NULL) {
	return;
    }

#define kProcessNeighborAdvertExpirationTimeSecs	60
    current_time = timer_current_secs();
    if ((current_time - S_wake_time)
	> kProcessNeighborAdvertExpirationTimeSecs) {
	my_log(LOG_INFO, 
	       "%@: tossing neighbor advert list (%g - %g) > (%d)",
	       ifstate->ifname,
	       current_time,
	       S_wake_time,
	       kProcessNeighborAdvertExpirationTimeSecs);
	/* information is stale, toss it */
	my_CFRelease(&ifstate->neighbor_advert_list);
	return;
    }
    if_p = ifstate->if_p;
    count = CFArrayGetCount(ifstate->neighbor_advert_list);
    for (i = 0; i < count; ) {
	struct in6_addr	address;
	CFStringRef	address_cf;
	int		error;
	boolean_t	remove = FALSE;

	address_cf = (CFStringRef)
	    CFArrayGetValueAtIndex(ifstate->neighbor_advert_list, i);
	if (my_CFStringToIPv6Address(address_cf, &address) == FALSE) {
	    /* failed to convert, remove bogus value */
	    my_log(LOG_NOTICE, "%@: bogus address value %@",
		   ifstate->ifname, address_cf);
	    remove = TRUE;
	}
	else if (inet6_addrlist_in6_addr_is_ready(addr_list_p, 
						  &address) == FALSE) {
	    /* address not assigned or is not ready */
	    my_log(LOG_NOTICE, "%@: address %@ not present/ready",
		   ifstate->ifname, address_cf);
	    remove = FALSE;
	}
	else {
	    /* address ready, send neighbor advert and remove from list */
	    remove = TRUE;
	    if (sockfd < 0) {
		sockfd = ICMPv6SocketOpen(FALSE);
		if (sockfd < 0) {
		    my_log_fl(LOG_ERR, "can't open socket, %s",
			      strerror(errno));
		    break;
		}
	    }
	    error 
		= ICMPv6SocketSendNeighborAdvertisement(sockfd,
							if_link_index(if_p),
							if_link_address(if_p),
							if_link_length(if_p),
							&address);
	    if (error != 0) {
		my_log(LOG_ERR,
		       "%s: failed to send neighbor advertisement, %s",
		       if_name(if_p), strerror(error));
	    }
	    else {
		char 	ntopbuf[INET6_ADDRSTRLEN];

		my_log(LOG_INFO,
		       "%s: sent neighbor advertisement for %s",
		       if_name(if_p),
		       inet_ntop(AF_INET6, &address, ntopbuf, sizeof(ntopbuf)));
	    }
	}
	if (remove) {
	    CFArrayRemoveValueAtIndex(ifstate->neighbor_advert_list, i);
	    count--;
	}
	else {
	    /* skip to next value */
	    i++;
	}
    }
    if (CFArrayGetCount(ifstate->neighbor_advert_list) == 0) {
	/* list is empty, toss it */
	my_CFRelease(&ifstate->neighbor_advert_list);
    }
    
    if (sockfd >= 0) {
	close(sockfd);
    }
    return;
}

STATIC CFMutableArrayRef
S_copy_neighbor_advert_list(SCDynamicStoreRef store, CFStringRef ifname)
{
    CFDictionaryRef		info;
    CFArrayRef			list;
    CFMutableArrayRef		ret = NULL;

    info = S_copy_sleep_proxy_info(store, ifname);
    if (info != NULL) {
	list = CFDictionaryGetValue(info, kBSPRegisteredAddresses);
	if (isA_CFArray(list) != NULL) {
	    my_log(LOG_INFO, "%@: Sleep Proxy Addresses = %@",
		   ifname, list);
	    ret = CFArrayCreateMutableCopy(NULL, CFArrayGetCount(list), list);
	}
	CFRelease(info);
    }
    return (ret);
}

/**
 ** Computer Name handling routines
 **/

PRIVATE_EXTERN char *
computer_name()
{
    return (S_computer_name);
}

/* 
 *	Try to shorten the length of the string by replacing certain
 *	product names.  If still not short enough, eliminate -'s.
 *	If still not short enough, remove enough of the middle part of
 *	the remaining string to make it 'desired_length'.
 */
static CFStringRef
myCFStringCopyShortenedString(CFStringRef computer_name, int desired_length)
{
    CFMutableStringRef short_name;
    CFIndex len, delete_len;

#define MINIMUM_SHORTENED_STRING_LENGTH 3 //  Min 3 chars <first part>-<last part>

    
    if (computer_name == NULL
	|| desired_length < MINIMUM_SHORTENED_STRING_LENGTH) {
	return NULL;
    }
    short_name  = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, computer_name);

    /*
     * shorten commonly used long names like MacBook-Pro, MacBook-Air
     */
    CFStringFindAndReplace(short_name, CFSTR("macbook-air"), CFSTR("Air"), CFRangeMake(0, CFStringGetLength(computer_name)), kCFCompareCaseInsensitive);
    CFStringFindAndReplace(short_name, CFSTR("macbook-pro"), CFSTR("MBP"), CFRangeMake(0, CFStringGetLength(computer_name)), kCFCompareCaseInsensitive);
    CFStringFindAndReplace(short_name, CFSTR("mac-mini"), CFSTR("Mini"), CFRangeMake(0, CFStringGetLength(computer_name)), kCFCompareCaseInsensitive);
    CFStringFindAndReplace(short_name, CFSTR("mac-pro"), CFSTR("Pro"), CFRangeMake(0, CFStringGetLength(computer_name)), kCFCompareCaseInsensitive);
    
    // recompute the new string length
    len = CFStringGetLength(short_name);
    if (len <= desired_length) {
	/* shrinking long names worked */
	return short_name;
    }

    CFStringFindAndReplace(short_name, CFSTR("-"), CFSTR(""), CFRangeMake(0, len-1), kCFCompareCaseInsensitive);
    // recompute the new string length
    len = CFStringGetLength(short_name);

    if (len <= desired_length) {
	/* computer name already short due to removal of '-'*/
	return short_name;
    }
	
    delete_len = len - desired_length;
    // last option: eliminate the middle string, keep first and last part of the string
    CFStringDelete(short_name,
		   CFRangeMake(desired_length / 2, delete_len));
    
    return short_name;

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
	    CFStringRef short_name = myCFStringCopyShortenedString(name, S_dhcp_local_hostname_length_max);
	    if (short_name == NULL) {
		goto done;
	    }
	    my_CFRelease(&name);
    	    name = short_name;
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

PRIVATE_EXTERN void
service_resolve_router_cancel(ServiceRef service_p, arp_client_t arp)
{
    if (arp != NULL) {
	arp_client_cancel(arp);
    }
    service_router_clear_resolve_in_progress(service_p);
    return;
}

static void
service_resolve_router_complete(void * arg1, void * arg2, 
				const arp_result_t * result)
{
    service_resolve_router_callback_t *	callback_func;
    interface_t *			if_p;
    ServiceRef				service_p;
    router_arp_status_t			status;

    service_p = (ServiceRef)arg1;
    service_router_clear_resolve_in_progress(service_p);
    callback_func = (service_resolve_router_callback_t *)arg2;
    if_p = service_interface(service_p);
    if (result->error) {
	my_log(LOG_NOTICE, "service_resolve_router_complete %s: ARP failed, %s",
	       if_name(if_p),
	       arp_client_errmsg(result->client));
	status = router_arp_status_failed_e;
    }
    else if (result->in_use) {
	/* grab the latest router hardware address */
	bcopy(result->addr.target_hardware, service_p->u.v4.router.hwaddr, 
	      if_link_length(if_p));
	service_router_set_all_valid(service_p);
	my_log(LOG_INFO, "service_resolve_router_complete %s: ARP "
	       IP_FORMAT ": response received", if_name(if_p),
	       IP_LIST(&service_p->u.v4.router.iaddr));
	status = router_arp_status_success_e;
    }
    else {
	status = router_arp_status_no_response_e;
	my_log(LOG_INFO, "service_resolve_router_complete %s: ARP router " 
	       IP_FORMAT ": no response", if_name(if_p),
	       IP_LIST(&service_p->u.v4.router.iaddr));
	service_router_set_resolve_timed_out(service_p);
    }
    (*callback_func)(service_p, status);
    return;
}

PRIVATE_EXTERN boolean_t
service_resolve_router(ServiceRef service_p, arp_client_t arp,
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
    service_router_set_resolve_in_progress(service_p);
    service_router_clear_resolve_timed_out(service_p);
    router_ip = service_router_iaddr(service_p);
    my_log(LOG_INFO, "service_resolve_router %s: sender " IP_FORMAT 
	   " target " IP_FORMAT " started", 
	   if_name(if_p), IP_LIST(&our_ip), IP_LIST(&router_ip));
    arp_client_resolve(arp, service_resolve_router_complete,
		       service_p, callback_func, our_ip, router_ip,
		       S_discover_router_mac_address_secs);
    return (TRUE);
}

PRIVATE_EXTERN boolean_t
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
    
    if (!service_router_is_iaddr_valid(service_p)) {
	my_log(LOG_INFO,
	       "%s: service_populate_router_arpinfo gateway missing", 
	       if_name(if_p));
	return (FALSE);
    }

    router_ip = service_router_iaddr(service_p);

    my_log(LOG_INFO,
	   "%s: service_populate_router_arpinfo found gateway " IP_FORMAT,
	   if_name(if_p), IP_LIST(&router_ip));

    info_p->target_ip = router_ip; 
    bcopy(service_router_hwaddr(service_p), info_p->target_hardware, 
	  service_router_hwaddr_size(service_p));
    
    return (TRUE);
}


PRIVATE_EXTERN boolean_t
service_update_router_address(ServiceRef service_p,
			      dhcpol_t * options, struct in_addr our_ip)
{
    struct in_addr		router;

    if (dhcp_get_router_address(options, our_ip, &router) == FALSE) {
	service_router_clear(service_p);
	return (FALSE);
    }
    if (service_router_all_valid(service_p)
	&& router.s_addr == service_router_iaddr(service_p).s_addr) {
	/* router is the same, no need to update */
	return (FALSE);
    }
    service_router_clear(service_p);
    service_router_set_iaddr(service_p, router);
    service_router_set_iaddr_valid(service_p);
    return (TRUE);
}

#define STARTUP_KEY	CFSTR("Plugin:IPConfiguration")

static __inline__ void
unblock_startup(SCDynamicStoreRef session)
{
    (void)SCDynamicStoreSetValue(session, STARTUP_KEY, STARTUP_KEY);
}

/**
 ** Active During Sleep (ADS) routines
 **/
INLINE CFStringRef
ActiveDuringSleepRequestedKeyCopy(CFStringRef ifn_cf)
{
    return (SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  ifn_cf,
							  kSCEntNetInterfaceActiveDuringSleepRequested));
}

STATIC void
ActiveDuringSleepRequestedKeyChanged(SCDynamicStoreRef store, 
				     CFStringRef cache_key)
{
    boolean_t		ads_requested;
    CFDictionaryRef	dict;
    CFStringRef		ifname;
    IFStateRef   	ifstate;

    if (CFStringHasPrefix(cache_key, S_state_interface_prefix) == FALSE) {
	return;
    }

    /* State:/Network/Interface/<ifname>/ActiveDuringSleepRequested */
    ifname = my_CFStringCopyComponent(cache_key, CFSTR("/"), 3);
    if (ifname == NULL) {
	return;
    }
    ifstate = IFStateListGetIFState(&S_ifstate_list, ifname, NULL);
    my_CFRelease(&ifname);
    if (ifstate != NULL) {
	dict = SCDynamicStoreCopyValue(store, cache_key);
	if (dict != NULL) {
	    ads_requested = TRUE;
	    CFRelease(dict);
	}
	else {
	    ads_requested = FALSE;
	}
	IFStateSetActiveDuringSleepRequested(ifstate, ads_requested);
    }
    return;
}

STATIC void
ActiveDuringSleepProcess(IFStateList_t * list)
{
    int 		i;
    int			if_count;

    my_log(LOG_DEBUG, "%s", __func__);
    if_count = dynarray_count(list);
    for (i = 0; i < if_count; i++) {
	IFStateRef		ifstate = dynarray_element(list, i);

	IFStateProcessActiveDuringSleep(ifstate);
    }
    return;
}

#define WAKE_ID_PREFIX		"com.apple.networking.IPConfiguration"

STATIC void
CleanupWakeEvents(void)
{
    CFArrayRef		events;
    CFIndex		count;
    int			i;

    events = IOPMCopyScheduledPowerEvents();
    if (events == NULL) {
	return;
    }
    count = CFArrayGetCount(events);
    for (i = 0; i < count; i++) {
	CFDictionaryRef	event = CFArrayGetValueAtIndex(events, i);
	CFStringRef	wake_id;

	wake_id = CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventAppNameKey));
	if (CFStringHasPrefix(wake_id, CFSTR(WAKE_ID_PREFIX))) {
	    CFDateRef	wake_time;

	    my_log(LOG_INFO, "IOPMCancelScheduledPowerEvent(%@)", wake_id);
	    wake_time 
		= CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventTimeKey));
	    IOPMCancelScheduledPowerEvent(wake_time, wake_id, 
					  CFSTR(kIOPMAutoWake));
	}
    }
    CFRelease(events);
    return;
}

/**
 ** DisableUntilNeeded functions
 **/

STATIC void
IFStateRankedListInsertIFState(ptrlist_t * ranked_list, IFStateRef ifstate)
{
    CFIndex		count;
    CFIndex		i;

    count = ptrlist_count(ranked_list);
    for (i = 0; i < count; i++) {
	IFStateRef	this_ifstate = ptrlist_element(ranked_list, i);

	if (ifstate->rank < this_ifstate->rank) {
	    ptrlist_insert(ranked_list, ifstate, i);
	    goto done;
	}
    }
    ptrlist_add(ranked_list, ifstate);

 done:
    return;
}

STATIC void
IFStateComputeRank(IFStateRef ifstate, CFArrayRef service_order)
{
    Rank	rank_v4;
    Rank	rank_v6;
    boolean_t	ready;

    IFStateFlagsClear(ifstate, kIFStateFlagsServicesReady);
    rank_v4 = service_list_get_rank(&ifstate->services, service_order,
				    &ready);
    if (ready) {
	IFStateFlagsSet(ifstate, kIFStateFlagsServicesReady);
    }
    rank_v6 = service_list_get_rank(&ifstate->services_v6, service_order,
				    &ready);
    if (ready) {
	IFStateFlagsSet(ifstate, kIFStateFlagsServicesReady);
    }
    ifstate->rank = (rank_v4 < rank_v6) ? rank_v4 : rank_v6;
    return;
}

STATIC void
IFStateRankedListPopulate(ptrlist_t * ranked_list, 
			  IFStateList_t * list, CFArrayRef service_order)
{
    CFIndex		count;
    CFIndex 		i;

    count = dynarray_count(list);
    for (i = 0; i < count; i++) {
	IFStateRef	ifstate = dynarray_element(list, i);

	IFStateComputeRank(ifstate, service_order);
	IFStateRankedListInsertIFState(ranked_list, ifstate);
    }
    return;
}

STATIC void
DisableUntilNeededProcess(IFStateList_t * list, CFArrayRef service_order)
{
    Boolean		better_interface_has_connectivity = FALSE;
    CFIndex		count;
    CFIndex 		i;
    ptrlist_t		ranked_list;

    my_log(LOG_DEBUG, "%s", __func__);
    ptrlist_init(&ranked_list);
    IFStateRankedListPopulate(&ranked_list, list, service_order);
    count = ptrlist_count(&ranked_list);
    for (i = 0; i < count; i++) {
	interface_t *	if_p;
	IFStateRef	ifstate = ptrlist_element(&ranked_list, i);

	if_p = ifstate->if_p;
	if (!IFStateGetDisableUntilNeededRequested(ifstate)
	    || better_interface_has_connectivity == FALSE) {
	    if (ifstate->disable_until_needed.interface_disabled) {
		ifstate->disable_until_needed.interface_disabled = FALSE;
		my_log(LOG_NOTICE,
		       "%s: marking interface up again", if_name(if_p));
		interface_up_down(if_name(if_p), TRUE);
		/* re-enable the interface if there are services defined */
		if (dynarray_count(&ifstate->services) > 0) {
		    inet_attach_interface(if_name(if_p), TRUE);
		}
		if (dynarray_count(&ifstate->services_v6) > 0) {
		    (void)IFState_attach_IPv6(ifstate, TRUE);
		}
	    }
	}
	else if (ifstate->disable_until_needed.interface_disabled == FALSE) {
	    my_log(LOG_NOTICE,
		   "%s: marking interface down", if_name(if_p));
	    interface_up_down(if_name(if_p), FALSE);
	    ifstate->disable_until_needed.interface_disabled = TRUE;
	}
	if (IFStateFlagsAreSet(ifstate, kIFStateFlagsServicesReady)) {
	    better_interface_has_connectivity = TRUE;
	}
    }
    ptrlist_free(&ranked_list);
    return;
}

STATIC void
setDisableUntilNeededNeedsAttention(void)
{
    if (S_disable_unneeded_interfaces) {
	S_disable_until_needed_needs_attention = TRUE;
	schedule_async_work();
    }
    return;
}

/**
 ** Service, IFState routines
 **/
static void
ServiceFree(void * arg)
{
    IFStateRef		ifstate;
    ServiceRef		service_p = (ServiceRef)arg;

    my_log(LOG_DEBUG, "ServiceFree(%@) %s", 
	   service_p->serviceID, ipconfig_method_string(service_p->method));
    ifstate = service_ifstate(service_p);
    if (ifstate != NULL && ifstate->linklocal_service_p == service_p) {
	ifstate->linklocal_service_p = NULL;
    }
    config_method_stop(service_p);
    service_publish_clear(service_p, ipconfig_status_success_e);
#if TARGET_OS_OSX
    ServiceRemoveAddressConflict(service_p);
#endif /* TARGET_OS_OSX */
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
    service_set_apn_name(service_p, NULL);
    free(service_p);
    return;
}

static ServiceRef
ServiceCreate(IFStateRef ifstate, CFStringRef serviceID,
	      ipconfig_method_info_t info,
	      ServiceRef parent_service_p,
	      ServiceInitHandler init_handler,
	      ipconfig_status_t * status_p)
{
    ipconfig_method_t	method = info->method;
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
    if (init_handler != NULL) {
	(init_handler)(service_p);
    }
    status = config_method_start(service_p, info);
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
    my_log(LOG_INFO, "IPConfiguration: pid %d exited", pid);
    serviceID = (CFStringRef)dispatch_get_context(source);
    status = S_remove_service_with_id_str(NULL, serviceID);
    if (status != ipconfig_status_success_e) {
	my_log(LOG_NOTICE,
	       "IPConfiguration: failed to stop service %@, %s",
	       serviceID, ipconfig_status_string(status));
    }
    return;
}

static void
ServiceMonitorPID(ServiceRef service_p, pid_t pid)
{
    dispatch_source_t		source;

    source = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, pid,
				    DISPATCH_PROC_EXIT,
				    IPConfigurationAgentQueue());
    if (source == NULL) {
	my_log(LOG_NOTICE, "IPConfiguration: dispatch_source_create failed");
	return;
    }
    CFRetain(service_p->serviceID);
    dispatch_set_context(source, (void *)service_p->serviceID);
    dispatch_source_set_event_handler(source,
				      ^{ ServiceHandleProcessExit(source); });
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
IFStateGetFirstRoutableService(IFStateRef ifstate)
{
    ServiceRef	service_p;

    service_p = service_list_first_routable_service(&ifstate->services);
    if (service_p) {
	return (service_p);
    }
    service_p = service_list_first_routable_service(&ifstate->services_v6);
    return (service_p);
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
				    ipconfig_method_info_t info,
				    boolean_t just_dynamic)
{
    int			i;
    boolean_t		is_dhcp_or_bootp = FALSE;
    boolean_t		is_manual;
    ipconfig_method_t	method = info->method;

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
		&& (info->method_data.manual.addr.s_addr
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
				    ipconfig_method_info_t info,
				    boolean_t just_dynamic)
{
    int			i;

    for (i = 0; i < dynarray_count(&ifstate->services_v6); i++) {
	ServiceRef	service_p = dynarray_element(&ifstate->services_v6, i);
	ServiceIPv6Ref	v6_p;
	
	if (just_dynamic && service_p->is_dynamic == FALSE) {
	    continue;
	}
	if (service_p->method != info->method) {
	    continue;
	}
	if (info->method != ipconfig_method_manual_v6_e) {
	    return (service_p);
	}
	v6_p = (ServiceIPv6Ref)service_p;
	if (IN6_ARE_ADDR_EQUAL(&info->method_data.manual_v6.addr,
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
				ipconfig_method_info_t info,
				boolean_t just_dynamic)
{
    if (ipconfig_method_is_v4(info->method)) {
	return (IFStateGetServiceMatchingIPv4Method(ifstate, info,
						    just_dynamic));
    }
    return (IFStateGetServiceMatchingIPv6Method(ifstate, info,
						just_dynamic));
}

static ServiceRef
IFStateGetServiceWithIPv4Method(IFStateRef ifstate,
				ipconfig_method_info_t info,
				boolean_t just_dynamic)
{
    int			j;
    boolean_t		is_manual;

    is_manual = ipconfig_method_is_manual(info->method);
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	ServiceRef	service_p = dynarray_element(&ifstate->services, j);

	if (just_dynamic && service_p->is_dynamic == FALSE) {
	    continue;
	}
	if (info->method == service_p->method) {
	    if (is_manual == FALSE
		|| (info->method_data.manual.addr.s_addr
		    == service_requested_ip_addr(service_p).s_addr)) {
		return (service_p);
	    }
	}
    }
    return (NULL);
}

static __inline__ ServiceRef
IFStateGetServiceWithIPv6Method(IFStateRef ifstate,
				ipconfig_method_info_t info,
				boolean_t just_dynamic)
{
    /* IFStateGetServiceMatchingIPv6Method is already an exact match */
    return (IFStateGetServiceMatchingIPv6Method(ifstate, info,
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
			    ipconfig_method_info_t info,
			    boolean_t just_dynamic)
{
    if (ipconfig_method_is_v4(info->method)) {
	return (IFStateGetServiceWithIPv4Method(ifstate, info,
						just_dynamic));
    }
    return (IFStateGetServiceWithIPv6Method(ifstate, info,
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
    int		count = dynarray_count(&ifstate->services);

    if (all) {
	dynarray_free(&ifstate->services);
    }
    else {
	S_FreeNonDynamicServices(&ifstate->services);
    }
    if (count != 0) {
	IFState_detach_IPv4(ifstate);
    }
    return;
}

static void
IFStateFreeIPv6Services(IFStateRef ifstate, boolean_t all)
{
    int		count = dynarray_count(&ifstate->services_v6);

    if (all) {
	dynarray_free(&ifstate->services_v6);
    }
    else {
	S_FreeNonDynamicServices(&ifstate->services_v6);
    }
    if (count != 0) {
	IFState_detach_IPv6(ifstate);
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
	    break;
	}
    }
    if (is_ipv4) {
	IFState_detach_IPv4(ifstate);
    }
    else {
	IFState_detach_IPv6(ifstate);
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
		    ipconfig_method_info_t info,
		    ServiceRef parent_service_p,
		    ServiceInitHandler init_handler,
		    ServiceRef * ret_service_p)
{
    interface_t *	if_p = ifstate->if_p;
    ipconfig_method_t	method = info->method;
    ServiceRef		service_p = NULL;
    ipconfig_status_t	status = ipconfig_status_success_e;

    if (ipconfig_method_is_v4(method)) {
	if (parent_service_p == NULL
	    && IFStateGetDisableUntilNeededRequested(ifstate)) {
	    if (ifstate->disable_until_needed.interface_disabled == FALSE) {
		ifstate->disable_until_needed.interface_disabled = TRUE;
		interface_up_down(if_name(if_p), FALSE);
	    }
	}
	else {
	    /* attach IP */
	    inet_attach_interface(if_name(if_p), TRUE);
	}
    }
    else {
	if (info->disable_cga) {
	    /* service doesn't want CGA */
	    my_log(LOG_INFO, "%s: CGA is disabled\n", if_name(if_p));
	    IFStateFlagsSet(ifstate, kIFStateFlagsDisableCGA);
	}

	if (IN6_IS_ADDR_LINKLOCAL(&info->ipv6_linklocal)) {
	    char 	ntopbuf[INET6_ADDRSTRLEN];

	    IFStateSetIPv6LinkLocalAddress(ifstate, &info->ipv6_linklocal);
	    my_log(LOG_INFO, "%s: link-local IPv6 address specified %s",
		   if_name(if_p),
		   inet_ntop(AF_INET6, &info->ipv6_linklocal,
			     ntopbuf, sizeof(ntopbuf)));
	}

	/* attach IPv6 */
	if (parent_service_p == NULL
	    && IFStateGetDisableUntilNeededRequested(ifstate)) {
	    if (ifstate->disable_until_needed.interface_disabled == FALSE) {
		ifstate->disable_until_needed.interface_disabled = TRUE;
		interface_up_down(if_name(if_p), FALSE);
	    }
	}
	else {
	    (void)IFState_attach_IPv6(ifstate, TRUE);
	}
    }

    /* try to configure the service */
    service_p = ServiceCreate(ifstate, serviceID, info,
			      parent_service_p, init_handler, &status);
    if (service_p == NULL) {
	my_log(LOG_INFO, "status from %s was %s",
	       ipconfig_method_string(method),
	       ipconfig_status_string(status));
	if (ipconfig_method_is_v4(method)) {
	    IFState_detach_IPv4(ifstate);
	}
	else {
	    IFState_detach_IPv6(ifstate);
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
	my_log(LOG_INFO, "%s link is unknown", ifname);
    }
    else {
	my_log(LOG_INFO, "%s link is %s", ifname, link.active ? "up" : "down");
    }
    if (if_is_wifi_infra(ifstate->if_p)) {
	WiFiInfoRef		info_p;

	info_p = S_copy_wifi_info(ifstate->ifname);
	IFState_set_wifi_info(ifstate, info_p);
	my_CFRelease(&info_p);
    }
    return;
}

STATIC void
IFState_active_during_sleep_init(IFStateRef ifstate)
{
    CFDictionaryRef	dict;
    CFStringRef		key;

    key = ActiveDuringSleepRequestedKeyCopy(ifstate->ifname);
    dict = SCDynamicStoreCopyValue(S_scd_session, key);
    CFRelease(key);
    if (dict != NULL) {
	ifstate->active_during_sleep.requested = TRUE;
	CFRelease(dict);
    }
    return;
}

static IFStateRef
IFState_init(interface_t * if_p)
{
    IFStateRef ifstate;

    ifstate = malloc(sizeof(*ifstate));
    if (ifstate == NULL) {
	my_log(LOG_NOTICE, "IFState_init: malloc ifstate failed");
	return (NULL);
    }
    bzero(ifstate, sizeof(*ifstate));
    ifstate->if_p = if_dup(if_p);
    ifstate->ifname = CFStringCreateWithCString(NULL, if_name(if_p),
						kCFStringEncodingASCII);
    IFState_update_media_status(ifstate);
    IFState_active_during_sleep_init(ifstate);
    ifstate->timer = timer_callout_init(if_name(if_p));
    ifstate->wake_generation = S_wake_generation;
    dynarray_init(&ifstate->services, ServiceFree, NULL);
    dynarray_init(&ifstate->services_v6, ServiceFree, NULL);
    return (ifstate);
}

static boolean_t
IFState_wifi_did_roam(IFStateRef ifstate, WiFiInfoRef info_p)
{
    WiFiInfoComparisonResult	result;

    result = WiFiInfoCompare(ifstate->wifi_info_p, info_p);
    return (result == kWiFiInfoComparisonResultBSSIDChanged);
}

static void
IFState_set_wifi_info(IFStateRef ifstate, WiFiInfoRef info_p)
{
    if (info_p != NULL) {
	CFRetain(info_p);
    }
    my_CFRelease(&ifstate->wifi_info_p);
    ifstate->wifi_info_p = info_p;
    return;
}

static void
IFState_free(void * arg)
{
    IFStateRef		ifstate = (IFStateRef)arg;

    my_log(LOG_DEBUG, "IFState_free(%s)", if_name(ifstate->if_p));
    IFStateFreeIPv4Services(ifstate, TRUE);
    IFStateFreeIPv6Services(ifstate, TRUE);
    my_CFRelease(&ifstate->ifname);
    my_CFRelease(&ifstate->neighbor_advert_list);
    IFState_set_wifi_info(ifstate, NULL);
    if_free(&ifstate->if_p);
    timer_callout_free(&ifstate->timer);
    free(ifstate);
    return;
}

STATIC const struct in6_addr *
IFStateIPv6LinkLocalAddress(IFStateRef ifstate)
{
    struct in6_addr *	addr = &ifstate->ipv6_linklocal;

    if (IN6_IS_ADDR_LINKLOCAL(addr)) {
	return (addr);
    }
    return (NULL);
}

STATIC void
IFStateSetIPv6LinkLocalAddress(IFStateRef ifstate,
			       const struct in6_addr * addr)
{
    ifstate->ipv6_linklocal = *addr;
    return;
}

STATIC void
IFStateClearIPv6LinkLocalAddress(IFStateRef ifstate)
{
    bzero(&ifstate->ipv6_linklocal, sizeof(ifstate->ipv6_linklocal));
}

STATIC void
IFState_detach_IPv4(IFStateRef ifstate)
{
    if (dynarray_count(&ifstate->services) == 0) {
	interface_t *	if_p = ifstate->if_p;

	IFStateFlagsSet(ifstate, kIFStateFlagsStartupReady);
	inet_detach_interface(if_name(if_p));
    }
    return;
}

STATIC void
IFState_detach_IPv6(IFStateRef ifstate)
{
    if (dynarray_count(&ifstate->services_v6) == 0) {
	interface_t *	if_p = ifstate->if_p;
	int		ift_type = if_ift_type(if_p);

	if (ift_type != IFT_LOOP && ift_type != IFT_STF) {
	    (void)inet6_rtadv_disable(if_name(if_p));
	    (void)inet6_linklocal_stop(if_name(if_p));
	    IFStateClearIPv6LinkLocalAddress(ifstate);
	    IFStateFlagsClear(ifstate, kIFStateFlagsDisableCGA);
	}
	inet6_detach_interface(if_name(if_p));
    }
    return;
}

STATIC boolean_t
IFState_attach_IPv6(IFStateRef ifstate, boolean_t set_iff_up)
{
    interface_t *		if_p = ifstate->if_p;
    int				ift_type = if_ift_type(if_p);
    const struct in6_addr *	ipv6_ll = NULL;
    boolean_t			loop_or_stf;
    boolean_t			proto_attached = FALSE;
    boolean_t			use_cga = FALSE;

    /*
     * Don't use CGA on an interface that is loopback, stf, or awdl. If not
     * one of those excluded interfaces, check if a specific IPV6 link-local
     * address (IID) is specified. If it is not, then use try to use CGA.
     */
    loop_or_stf = (ift_type == IFT_LOOP) || (ift_type == IFT_STF);
    if (!loop_or_stf && !if_is_awdl(if_p)) {
	ipv6_ll = IFStateIPv6LinkLocalAddress(ifstate);
	use_cga = !IFStateFlagsAreSet(ifstate, kIFStateFlagsDisableCGA);
    }
    if (inet6_attach_interface(if_name(if_p), set_iff_up) == 0) {
	proto_attached = TRUE;
    }

    /* only start IPv6 link-local on non-{loopback, stf} */
    if (!loop_or_stf) {
	link_status_t		link = if_get_link_status(if_p);

	if (link.valid == FALSE || link.active) {
	    boolean_t	perform_nud;
	    boolean_t	dad;

	    /* start IPv6 Link Local */
	    perform_nud
		= !IFStateFlagsAreSet(ifstate, kIFStateFlagsDisablePerformNUD);
	    dad = !IFStateFlagsAreSet(ifstate, kIFStateFlagsDisableDAD);
	    (void)inet6_linklocal_start(if_name(if_p), ipv6_ll,
					perform_nud,
					use_cga,
					dad,
					ifstate->v6_ll_collision_count);
	}
    }
    return (proto_attached);
}

STATIC void
IFStateReportState(IFStateRef ifstate,
		   CFStringRef entity, boolean_t set)
{
    CFDictionaryRef	dict;

    if (set) {
	dict = CFDictionaryCreate(NULL, NULL, NULL, 0,
				  &kCFTypeDictionaryKeyCallBacks,
				  &kCFTypeDictionaryValueCallBacks);
    }
    else {
	dict = NULL;
    }
    my_SCDynamicStoreSetInterface(S_scd_session, ifstate->ifname, entity, dict);
    my_CFRelease(&dict);
    schedule_async_work();
    return;
}

STATIC void
IFStateSetIPv4PublishNeedsAttention(IFStateRef ifstate)
{
    S_ipv4_publish_needs_attention = TRUE;
    IFStateFlagsSet(ifstate, kIFStateFlagsIPv4PublishNeedsAttention);
    schedule_async_work();
    return;
}

STATIC void
IFStateSetActiveDuringSleepNeedsAttention(IFStateRef ifstate)
{
    S_active_during_sleep_needs_attention = TRUE;
    ifstate->active_during_sleep.needs_attention = TRUE;
    schedule_async_work();
    return;
}

STATIC void
IFStateSetActiveDuringSleepRequested(IFStateRef ifstate, boolean_t requested)
{
    if (requested != ifstate->active_during_sleep.requested) {
	/* active during sleep request changed */
	my_log(LOG_INFO, "%s: active during sleep %srequested",
	       if_name(ifstate->if_p), requested ? "" : "not ");
	ifstate->active_during_sleep.requested = requested;
	IFStateSetActiveDuringSleepNeedsAttention(ifstate);
    }
    return;
}

STATIC void
IFStateProcessActiveDuringSleep(IFStateRef ifstate)
{
    active_during_sleep_t	active_during_sleep;
    boolean_t			sleep_supported;

    if (ifstate->active_during_sleep.needs_attention == FALSE) {
	return;
    }
    ifstate->active_during_sleep.needs_attention = FALSE;
    bzero(&active_during_sleep, sizeof(active_during_sleep));
    active_during_sleep.requested 
	= ifstate->active_during_sleep.requested;
    active_during_sleep.supported = TRUE;

    /* v4 services */
    service_list_event(&ifstate->services, IFEventID_active_during_sleep_e,
		       &active_during_sleep);
#if 0
    /* v6 services */
    service_list_event(&ifstate->services_v6, IFEventID_active_during_sleep_e,
		       &active_during_sleep);
#endif
    /* indicate whether sleep is supported */
    sleep_supported = ifstate->active_during_sleep.requested
	&& active_during_sleep.supported;
    IFStateReportState(ifstate,
		       kSCEntNetInterfaceActiveDuringSleepSupported,
		       sleep_supported);
    return;
}

STATIC boolean_t
IFStateGetDisableUntilNeededRequested(IFStateRef ifstate)
{
    boolean_t		requested = FALSE;

    if (S_disable_unneeded_interfaces) {
	DisableUntilNeededInfoRef	dun_p = &ifstate->disable_until_needed;

	if (dun_p->prefs_set) {
	    requested = dun_p->prefs_requested;
	}
#if TARGET_OS_OSX
	else {
	    requested = if_is_tethered(ifstate->if_p);
	}
#endif /* TARGET_OS_OSX */
    }
    return (requested);
}

STATIC void
IFStateSetDisableUntilNeededRequested(IFStateRef ifstate,
				      CFBooleanRef requested_cf)
{
    DisableUntilNeededInfoRef	dun_p = &ifstate->disable_until_needed;
    boolean_t			old_requested;
    boolean_t			requested = FALSE;

    if (!S_disable_unneeded_interfaces) {
	return;
    }
    old_requested = IFStateGetDisableUntilNeededRequested(ifstate);
    if (requested_cf != NULL) {
	dun_p->prefs_set = TRUE;
	requested = dun_p->prefs_requested = CFBooleanGetValue(requested_cf);
    }
    else {
	dun_p->prefs_set = FALSE;
#if TARGET_OS_OSX
	requested = if_is_tethered(ifstate->if_p);
#endif /* TARGET_OS_OSX */
    }
    if (requested != old_requested) {
	/* disable until needed request changed */
	my_log(LOG_INFO,
	       "%s: disable until needed %srequested",
	       if_name(ifstate->if_p),
	       requested ? "" : "not ");
	setDisableUntilNeededNeedsAttention();
    }
    return;
}

STATIC CFDictionaryRef
IFStateCopySummary(IFStateRef ifstate)
{
    interface_t *		if_p = ifstate->if_p;
    CFMutableDictionaryRef	if_summary;
    const char *		if_type_str = NULL;
    link_status_t		link_status;
    dynarray_t *		list[] = {
					  &ifstate->services,
					  &ifstate->services_v6,
					  NULL
    };
    CFStringRef			list_entity[] = {
						 kSCEntNetIPv4,
						 kSCEntNetIPv6,
    };
    int				s;
    dynarray_t * *		scan;

    if_summary
	= CFDictionaryCreateMutable(NULL, 0,
				    &kCFTypeDictionaryKeyCallBacks,
				    &kCFTypeDictionaryValueCallBacks);
    for (s = 0, scan = list; *scan != NULL; scan++, s++) {
        int			count;
        int			i;
        CFMutableArrayRef	services = NULL;

        count = dynarray_count(*scan);
        for (i = 0; i < count; i++) {
            ServiceRef		service_p = dynarray_element(*scan, i);
            CFDictionaryRef	summary;

            summary = ServiceCopySummary(service_p);
            if (summary != NULL) {
                if (services == NULL) {
                    services
			= CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
                }
                CFArrayAppendValue(services, summary);
                CFRelease(summary);
            }
        }
        if (services != NULL) {
            CFDictionarySetValue(if_summary, list_entity[s], services);
            my_CFRelease(&services);
        }
    }
    link_status = if_get_link_status(if_p);
    if (link_status.valid) {
        if (link_status.active) {
	    WiFiInfoRef 	info_p = ifstate->wifi_info_p;

            CFDictionarySetValue(if_summary, CFSTR("LinkStatusActive"),
                                 kCFBooleanTrue);
            if (if_is_wifi_infra(if_p) && info_p != NULL) {
		const char *	auth_type;
		CFStringRef	networkID;

		auth_type = WiFiAuthTypeGetString(WiFiInfoGetAuthType(info_p));
		my_CFDictionarySetCString(if_summary, CFSTR("Security"),
					  auth_type);
		CFDictionarySetValue(if_summary, CFSTR("BSSID"),
				     WiFiInfoGetBSSIDString(info_p));
		CFDictionarySetValue(if_summary, CFSTR("SSID"),
				     WiFiInfoGetSSID(info_p));
		networkID = WiFiInfoGetNetworkID(info_p);
		if (networkID != NULL) {
		    CFDictionarySetValue(if_summary, CFSTR("NetworkID"),
					 networkID);
		}
            }
        }
        else {
            CFDictionarySetValue(if_summary, kSCPropNetLinkActive,
                                 kCFBooleanFalse);
        }
    }
    if_type_str = if_type_string(if_p);
    if (if_type_str != NULL) {
        my_CFDictionarySetCString(if_summary, CFSTR("InterfaceType"),
                                  if_type_str);
    }
    if (if_is_expensive(if_p)) {
        CFDictionarySetValue(if_summary, CFSTR("IsExpensive"),
                             kCFBooleanTrue);
    }
    if (if_is_tethered(if_p)) {
        CFDictionarySetValue(if_summary, CFSTR("IsTethered"),
                             kCFBooleanTrue);
    }
    return (if_summary);
}

STATIC const char *
get_busy_string(boolean_t busy)
{
    return busy ? "busy" : "not busy";
}

STATIC void
IFStateProcessBusy(IFStateRef ifstate)
{
    boolean_t	current_busy;
    boolean_t	busy;

    busy = service_list_check_busy(&ifstate->services)
	|| service_list_check_busy(&ifstate->services_v6);
    current_busy = IFStateFlagsAreSet(ifstate, kIFStateFlagsBusy);
    my_log(LOG_DEBUG, "%s: %s current %s requested %s",
	   __func__, if_name(ifstate->if_p),
	   get_busy_string(current_busy), get_busy_string(busy));
    if (busy != current_busy) {
	IFStateFlagsSetOrClear(ifstate, kIFStateFlagsBusy, busy);
	my_log(LOG_INFO, "%s: %s", if_name(ifstate->if_p),
	       get_busy_string(busy));
	IFStateReportState(ifstate,
			   kSCEntNetInterfaceIPConfigurationBusy,
			   IFStateFlagsAreSet(ifstate, kIFStateFlagsBusy));
    }
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
IFStateListGetIFState(IFStateList_t * list, CFStringRef ifname,
		      int * where)
{
    int i;

    for (i = 0; i < dynarray_count(list); i++) {
	IFStateRef	element = dynarray_element(list, i);
	
	if (CFEqual(ifname, element->ifname)) {
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

PRIVATE_EXTERN boolean_t 
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
		return (TRUE);
            }
	}
    }
    return (FALSE);

}

/**
 ** netboot-specific routines
 **/
PRIVATE_EXTERN void
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
    length = (int)CFDataGetLength(response);
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
    IFStateFlagsSet(ifstate, kIFStateFlagsNetBoot);
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
set_entity_value(CFTypeRef * entities,
		 CFDictionaryRef * values, int size,
		 CFTypeRef entity, CFDictionaryRef value,
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

PRIVATE_EXTERN const char *
ServiceGetMethodString(ServiceRef service_p)
{
    return (ipconfig_method_string(service_p->method));
}

static void
service_clear(ServiceRef service_p, ipconfig_status_t status)
{
    service_p->ready = FALSE;
    service_p->status = status;
    return;
}


STATIC CFStringRef
create_entities_summary(CFTypeRef * entities,
			CFDictionaryRef * values,
			CFIndex count)
{
    CFIndex		added_count;
    CFMutableStringRef	str;

    str = CFStringCreateMutable(NULL, 0);
    added_count = 0;
    for (CFIndex i = 0; i < count; i++) {
	CFTypeRef	entity = entities[i];

	if (values[i] == NULL) {
	    continue;
	}
	if (entity == kServiceEntity) {
	    /* service dictionary containing RankLast */
	    entity = CFSTR("RankLast");
	}
	added_count++;
	STRING_APPEND(str, "%s%@",
		      (added_count != 1) ? ", " : "",
		      entity);
    }
    return (str);
}

#define N_PUBLISH_ENTITIES	8

static void
service_publish_clear(ServiceRef service_p, ipconfig_status_t status)
{
    CFDictionaryRef	dns_dict = NULL;
    CFDictionaryRef	capport_dict = NULL;
    CFTypeRef		entities[N_PUBLISH_ENTITIES];
    int			entity_count;
    CFDictionaryRef	values[N_PUBLISH_ENTITIES];

    service_clear(service_p, status);
    if (S_scd_session == NULL) {
	return;
    }
    if (ServiceIsIPv4(service_p)) {
	/* IPv4 */
	entity_count = 0;
	if (!service_clat46_is_active(service_p)) {
	    set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			     kSCEntNetIPv4, NULL, &entity_count);
	}
	dns_dict = ServiceIPv4CopyMergedDNSAndCaptive(service_p, NULL,
						      &capport_dict);
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kSCEntNetDNS, dns_dict, &entity_count);
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kSCEntNetCaptivePortal, capport_dict, &entity_count);
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kSCEntNetDHCP, NULL, &entity_count);
#if TARGET_OS_OSX
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kSCEntNetSMB, NULL, &entity_count);
#endif /* TARGET_OS_OSX */
	ServiceSetActiveDuringSleepNeedsAttention(service_p);
	ServiceSetIPv4PublishNeedsAttention(service_p);
    }
    else {
	/* IPv6 */
	entity_count = 0;
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kSCEntNetIPv6, NULL, &entity_count);
	/* CLAT46 is active, IPv4 also needs removing */
	if (service_clat46_is_active(service_p)) {
	    set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			     kSCEntNetIPv4, NULL, &entity_count);
	}
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kSCEntNetDHCPv6, NULL, &entity_count);
	dns_dict = ServiceIPv6CopyMergedDNSAndCaptive(service_p, NULL,
						      &capport_dict);
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kSCEntNetDNS, dns_dict, &entity_count);
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kSCEntNetCaptivePortal, capport_dict, &entity_count);
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kServiceEntity, NULL, &entity_count);
#ifndef kSCEntNetPvD
#define  kSCEntNetPvD	CFSTR("PvD")
#endif
        set_entity_value(entities, values, N_PUBLISH_ENTITIES,
                         kSCEntNetPvD, NULL, &entity_count);
    }
    my_SCDynamicStoreSetService(S_scd_session,
				service_p->serviceID,
				entities, values, entity_count,
				service_p->no_publish);
    schedule_async_work();
    my_CFRelease(&dns_dict);
    my_CFRelease(&capport_dict);
    setDisableUntilNeededNeedsAttention();

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
	    && !IFStateFlagsAreSet(ifstate, kIFStateFlagsStartupReady)) {
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
    char			link_addr[LINK_ADDR_STR_LEN(MAX_LINK_ADDR_LEN)];
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
ServiceIPv4CopyMergedDNSAndCaptive(ServiceRef service_p,
				   dhcp_info_t * info_p,
				   CFDictionaryRef *capport_dict_p)
{
    CFDictionaryRef	dns_dict;
    interface_t *	if_p = service_interface(service_p);
    ipv6_info_t		info_v6;
    ServiceRef		ipv6_service_p;

    ipv6_service_p = IFStateGetServiceWithID(service_p->ifstate, 
					     service_p->serviceID, 
					     IS_IPV6);
    bzero(&info_v6, sizeof(info_v6));
    if (ipv6_service_p != NULL && ServiceIsPublished(ipv6_service_p)) {
	(void)config_method_event(ipv6_service_p, IFEventID_get_ipv6_info_e,
				  &info_v6);
    }
    dns_dict = DNSEntityCreateWithInfo(if_name(if_p), info_p, &info_v6);
    *capport_dict_p = CaptivePortalEntityCreateWithInfo(info_p, &info_v6);
    return (dns_dict);
}

#ifndef kSCPropConfirmedInterfaceName
#define kSCPropConfirmedInterfaceName CFSTR("ConfirmedInterfaceName")
#endif /* kSCPropConfirmedInterfaceName */

STATIC void
dict_insert_additional_routes(CFMutableDictionaryRef dict,
			      struct in_addr addr,
			      IPv4ClasslessRouteRef list,
			      int list_count)
{
    struct in_addr	linklocal_mask = { htonl(IN_CLASSB_NET) };
    struct in_addr	linklocal_network = { htonl(IN_LINKLOCALNETNUM) };
    CFDictionaryRef	route_dict;
    CFMutableArrayRef	routes;

    routes = CFArrayCreateMutable(NULL,
				  list_count + 2,
				  &kCFTypeArrayCallBacks);

    /* add interface address route */
    route_dict = route_dict_create(&addr, &G_ip_broadcast, NULL);
    CFArrayAppendValue(routes, route_dict);
    CFRelease(route_dict);

    /* add IPv4 link-local (169.254/16) route */
    if (!in_subnet(linklocal_network, linklocal_mask, addr)) {
	    /* add IPv4LL route */
	    route_dict = route_dict_create(&linklocal_network,
					   &linklocal_mask, NULL);
	    CFArrayAppendValue(routes, route_dict);
	    CFRelease(route_dict);
    }

    /* add classless routes */
    if (list != NULL) {
	int		i;

	for (i = 0; i < list_count; i++) {
	    struct in_addr	dest;
	    struct in_addr	mask;
	    struct in_addr *	gateway_p;

	    gateway_p = &list[i].gate;
	    if (gateway_p->s_addr == 0) {
		gateway_p = NULL;
	    }
	    mask.s_addr = htonl(prefix_to_mask32(list[i].prefix_length));
	    dest = list[i].dest;
	    dest.s_addr &= mask.s_addr;
	    route_dict = route_dict_create(&dest, &mask, gateway_p);
	    CFArrayAppendValue(routes, route_dict);
	    CFRelease(route_dict);
	}
    }
    CFDictionarySetValue(dict, kSCPropNetIPv4AdditionalRoutes, routes);
    CFRelease(routes);
    return;
}

STATIC void
service_scrub_old_ipv4_addresses(ServiceRef service_p)
{
    int			count;
    int			i;
    interface_t *	if_p = service_interface(service_p);
    IFStateRef		ifstate = service_ifstate(service_p);
    ServiceIPv4Ref	ll_v4_p = NULL;
    boolean_t		logged = FALSE;
    ServiceIPv4Ref	v4_p = &service_p->u.v4;

    if (IFStateFlagsAreSet(ifstate, kIFStateFlagsIPv4AddressesScrubbed)) {
	return;
    }
    IFStateFlagsSet(ifstate, kIFStateFlagsIPv4AddressesScrubbed);

    /* this function only handles a single routable service */
    count = dynarray_count(&ifstate->services);
    if (ifstate->linklocal_service_p != NULL) {
	ll_v4_p = &ifstate->linklocal_service_p->u.v4;
	/* only handle a routable service + IPv4LL */
	if (count > 2) {
	    return;
	}
    }
    else {
	/* only handle a single routable service */
	if (count > 1) {
	    return;
	}
    }

    /* scrub any address that isn't our routable or IPv4LL address */
    count = if_inet_count(if_p);
    for (i = 0; i < count; i++) {
	inet_addrinfo_t *	info_p = if_inet_addr_at(if_p, i);

	if (info_p->addr.s_addr == v4_p->info.addr.s_addr) {
	    /* it's our routable IP address, leave it alone */
	}
	else if (ll_v4_p != NULL
		 && info_p->addr.s_addr == ll_v4_p->info.addr.s_addr) {
	    /* it's our IPv4LL address, leave it alone */
	}
	else {
	    if (!logged) {
		my_log(LOG_NOTICE, "%s: removing stale IP address(es)",
		       if_name(if_p));
		logged = TRUE;
	    }
	    S_remove_ip_address(if_name(if_p), info_p->addr);
	}
    }
}

PRIVATE_EXTERN void
ServiceSetBusy(ServiceRef service_p, boolean_t busy)
{
    if (service_p->busy != busy) {
	service_p->busy = busy;
	my_log(LOG_INFO, "%s %s: %s",
	       ServiceGetMethodString(service_p),
	       if_name(service_interface(service_p)),
	       get_busy_string(busy));
	IFStateProcessBusy(service_ifstate(service_p));
    }
}

PRIVATE_EXTERN void
ServiceDetachIPv4(ServiceRef service_p)
{
    IFStateRef		ifstate = service_ifstate(service_p);

    IFState_detach_IPv4(ifstate);
    return;
}

PRIVATE_EXTERN void
ServicePublishSuccessIPv4(ServiceRef service_p, dhcp_info_t * dhcp_info_p)
{
    CFDictionaryRef		dhcp_dict = NULL;
    CFDictionaryRef		dns_dict = NULL;
    CFDictionaryRef		capport_dict = NULL;
    CFTypeRef			entities[N_PUBLISH_ENTITIES];
    CFStringRef			entities_summary;
    int				entity_count;
    interface_t *		if_p = service_interface(service_p);
    IFStateRef			ifstate = service_ifstate(service_p);
    inet_addrinfo_t *		info_p;
    CFMutableDictionaryRef	ipv4_dict = NULL;
    dhcpol_t *			options = NULL;
    ServiceRef			parent_service_p = NULL;
    IPv4ClasslessRouteRef	routes = NULL;
    int				routes_count = 0;
    CFStringRef			serviceID;
#if TARGET_OS_OSX
    CFMutableDictionaryRef	smb_dict = NULL;
    const uint8_t *		smb_nodetype = NULL;
    int				smb_nodetype_len = 0;
    struct in_addr *		smb_server = NULL;
    int				smb_server_len = 0;
#endif /* TARGET_OS_OSX */
    ServiceRef			the_service_p;
    CFDictionaryRef		values[N_PUBLISH_ENTITIES];
    boolean_t			was_published;

    if (service_p->serviceID == NULL) {
	return;
    }
    info_p = &service_p->u.v4.info;
    was_published = ServiceIsPublished(service_p);
    ServiceSetIsPublished(service_p);
    if (S_scd_session == NULL) {
	/* configd is not running */
	return;
    }
    if (!was_published) {
	/* we're publishing now, so deliver notification */
	ServiceSetIPv4PublishNeedsAttention(service_p);
    }
    if (dhcp_info_p != NULL) {
	options = dhcp_info_p->options;
    }
    if (service_p->parent_serviceID != NULL) {
	parent_service_p 
	    = IFStateGetServiceWithID(ifstate,
				      service_p->parent_serviceID,
				      IS_IPV4);
	if (parent_service_p == NULL
	    || parent_service_p->u.v4.info.addr.s_addr != 0) {
	    return;
	}
	if (service_p->method == ipconfig_method_linklocal_e
	    && IFStateFlagsAreSet(ifstate, kIFStateFlagsCLAT46Active)) {
	    my_log(LOG_NOTICE,
		   "%s: not publishing IPv4LL service, CLAT46 is active",
		   if_name(if_p));
	    return;
	}
	serviceID = service_p->parent_serviceID;
    }
    else {
	serviceID = service_p->serviceID;
    }

    /* IPv4 */
    ipv4_dict = CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
    /* Addresses */
    my_CFDictionarySetIPAddressAsArrayValue(ipv4_dict,
					    kSCPropNetIPv4Addresses,
					    info_p->addr);
    /* SubnetMasks */
    my_CFDictionarySetIPAddressAsArrayValue(ipv4_dict,
					    kSCPropNetIPv4SubnetMasks,
					    info_p->mask);

    /* InterfaceName */
    CFDictionarySetValue(ipv4_dict, kSCPropInterfaceName, ifstate->ifname);

    if (IFStateFlagsAreSet(ifstate, kIFStateFlagsNetBoot)
	&& service_p->parent_serviceID == NULL) {
	CFNumberRef	primary;
	int		enabled = 1;

	/* ensure that we're the primary service */
	primary = CFNumberCreate(NULL, kCFNumberIntType, &enabled);
	CFDictionarySetValue(ipv4_dict, kSCPropNetOverridePrimary,
			     primary);
	CFRelease(primary);
    }

    if (options == NULL) {
	/* manual config, check for router */
	if (service_router_is_iaddr_valid(service_p)) {
	    struct in_addr	router;

	    router = service_router_iaddr(service_p);
	    if (router.s_addr != INADDR_ANY
		&& router.s_addr != INADDR_BROADCAST) {
		my_CFDictionarySetIPAddressAsString(ipv4_dict,
						    kSCPropNetIPv4Router,
						    router);
	    }
	}
    }
    else {
	char *			host_name = NULL;
	int			host_name_len = 0;
	struct in_addr *	router = NULL;

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
	    router = (struct in_addr *)
		dhcpol_find_with_length(options,
					dhcptag_router_e,
					sizeof(*router));
	}
	if (dhcp_parameter_is_ok(dhcptag_classless_static_route_e)) {
	    routes = dhcp_copy_classless_routes(options, &routes_count);
	    if (router == NULL && routes != NULL) {
		IPv4ClasslessRouteRef def_route;

		def_route = IPv4ClasslessRouteListGetDefault(routes,
							     routes_count);
		if (def_route != NULL) {
		    router = &def_route->gate;
		}
	    }
	}
	/* set the router */
	if (router != NULL) {
	    struct in_addr	ip;

	    memcpy(&ip, router, sizeof(ip)); 	/* avoid alignment issues */
	    if (ip.s_addr == INADDR_ANY || ip.s_addr == INADDR_BROADCAST) {
		/* skip bad IP */
		my_log(LOG_NOTICE,
		       "%s: ignoring invalid router " IP_FORMAT,
		       if_name(if_p), IP_LIST(&ip));
	    }
	    else {
		my_CFDictionarySetIPAddressAsString(ipv4_dict,
						    kSCPropNetIPv4Router,
						    ip);
	    }
	}
    }

    if ((if_flags(if_p) & (IFF_LOOPBACK | IFF_POINTOPOINT)) == 0) {
	/* insert the signature */
	dict_insert_router_info(service_p, ipv4_dict);
	
	/* AdditionalRoutes */
	dict_insert_additional_routes(ipv4_dict, info_p->addr,
				      routes, routes_count);

	/* ConfirmedInterfaceName */
	CFDictionarySetValue(ipv4_dict, kSCPropConfirmedInterfaceName,
			     ifstate->ifname);
    }

    if (routes != NULL) {
	free(routes);
	routes = NULL;
    }

    /*
     * Entity values can be NULL or not NULL.  The values are accumulated in
     * the "values" array.
     */
    entity_count = 0;

    /* IPv4 */
    set_entity_value(entities, values, N_PUBLISH_ENTITIES, 
		     kSCEntNetIPv4, ipv4_dict, &entity_count);

    /* DNS and Captive Portal */
    the_service_p = (parent_service_p != NULL) ? parent_service_p : service_p;
    dns_dict = ServiceIPv4CopyMergedDNSAndCaptive(the_service_p,
						  dhcp_info_p,
						  &capport_dict);
    set_entity_value(entities, values, N_PUBLISH_ENTITIES,
		     kSCEntNetDNS, dns_dict, &entity_count);
    set_entity_value(entities, values, N_PUBLISH_ENTITIES,
		     kSCEntNetCaptivePortal, capport_dict, &entity_count);
#if TARGET_OS_OSX
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
#endif /* TARGET_OS_OSX */
    
    /* DHCP */
    if (dhcp_info_p != NULL && dhcp_info_p->pkt_size != 0) {
	dhcp_dict = DHCPInfoDictionaryCreate(service_p->method, 
					     dhcp_info_p->options,
					     dhcp_info_p->lease_start,
					     dhcp_info_p->lease_expiration);
    }
    set_entity_value(entities, values, N_PUBLISH_ENTITIES,
		     kSCEntNetDHCP, dhcp_dict, &entity_count);

    /* service */
    if (service_p->method != ipconfig_method_linklocal_e) {
	/* remove Rank */
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kServiceEntity, NULL, &entity_count);
    }

    my_SCDynamicStoreSetService(S_scd_session,
				serviceID,
				entities, values, entity_count,
				service_p->no_publish);
    schedule_async_work();
    entities_summary = create_entities_summary(entities, values, entity_count);
    my_CFRelease(&ipv4_dict);
    my_CFRelease(&dns_dict);
    my_CFRelease(&capport_dict);
    my_CFRelease(&dhcp_dict);
#if TARGET_OS_OSX
    my_CFRelease(&smb_dict);
#endif /* TARGET_OS_OSX */
    all_services_ready();
    ServiceSetActiveDuringSleepNeedsAttention(service_p);
    setDisableUntilNeededNeedsAttention();
    if (ipconfig_method_routable(service_p->method)
	&& IFStateFlagsAreSet(ifstate, kIFStateFlagsFailureSymptomReported)) {
	report_address_acquisition_symptom(if_link_index(ifstate->if_p), true);
	IFStateFlagsClear(ifstate, kIFStateFlagsFailureSymptomReported);
	my_log(LOG_NOTICE,
	       "%s %s: reported address acquisition success symptom",
	       ipconfig_method_string(service_p->method),
	       if_name(ifstate->if_p));
    }
    my_log(LOG_NOTICE,
	   "%s %s: publish success { %@ }",
	   ipconfig_method_string(service_p->method),
	   if_name(ifstate->if_p),
	   entities_summary);
    my_CFRelease(&entities_summary);
    if (service_p->parent_serviceID == NULL) {
	service_scrub_old_ipv4_addresses(service_p);
    }
    return;
}

STATIC void
send_opt_record(interface_t * if_p, arp_collision_data_t * arpc)
{
    int 	send_buf_aligned[512];
    char * 	send_buf = (char *)send_buf_aligned;
    int		status;

    if (arpc->opt_record == NULL) {
	return;
    }
#define MDNS_PORT	5353
    status = udpv4_transmit(-1,
			    send_buf,
			    if_name(if_p),
			    if_link_arptype(if_p),
			    arpc->hwaddr,
			    arpc->sleep_proxy_ip,
			    arpc->ip_addr,
			    MDNS_PORT,
			    MDNS_PORT,
			    CFDataGetBytePtr(arpc->opt_record),
			    CFDataGetLength(arpc->opt_record));
    my_log(LOG_INFO, "%s: OPT record sent (status %d)", if_name(if_p),
	   status);
    return;
}

PRIVATE_EXTERN boolean_t
ServiceDefendIPv4Address(ServiceRef service_p, arp_collision_data_t * arpc)
{
    absolute_time_t 		current_time;
    boolean_t			defended = FALSE;
    ServiceIPv4Ref		v4_p = &service_p->u.v4;

    current_time = timer_current_secs();
    if (arpc->is_sleep_proxy
	|| ((current_time - v4_p->ip_assigned_time) >
	    S_defend_ip_address_interval_secs)) {
	if (v4_p->ip_conflict_count > 0
	    && ((current_time - v4_p->ip_conflict_time)
		> S_defend_ip_address_interval_secs)) {
	    /*
	     * if it's been awhile since we last had to defend
	     * our IP address, assume we defended it successfully
	     * and start the conflict counter over again
	     */
	    v4_p->ip_conflict_count = 0;
	}
	v4_p->ip_conflict_time = current_time;
	v4_p->ip_conflict_count++;
	if (v4_p->ip_conflict_count > S_defend_ip_address_count) {
	    /* too many conflicts */
	}
	else {
	    arp_client_t	arp;
	    interface_t *	if_p = service_interface(service_p);

	    arp = arp_client_init(if_p);
	    if (arp == NULL) {
		my_log(LOG_NOTICE,
		       "IPConfiguration: "
		       "ServiceDefendIPv4Address arp_client_init failed");
	    }
	    else {
		/* tell the sleep proxy that we're taking ownership again */
		send_opt_record(if_p, arpc);

		/* send ARP defense packets */
		defended = arp_client_defend(arp, v4_p->info.addr);
		arp_client_free(&arp);
		my_log(LOG_NOTICE, "%s %s: defending IP "
		       IP_FORMAT
		       " against %s"
		       EA_FORMAT
		       " %d (of %d)",
		       ServiceGetMethodString(service_p), if_name(if_p),
		       IP_LIST(&v4_p->info.addr),
		       arpc->is_sleep_proxy ? "BonjourSleepProxy " : "",
		       EA_LIST(arpc->hwaddr),
		       v4_p->ip_conflict_count, S_defend_ip_address_count);
	    }
	}
    }
    return (defended);
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
ServiceIPv6CopyMergedDNSAndCaptive(ServiceRef service_p,
				   ipv6_info_t * info_v6_p,
				   CFDictionaryRef * capport_dict_p)
{
    CFDictionaryRef	dns;
    interface_t *	if_p = service_interface(service_p);
    dhcp_info_t		info;
    ServiceRef		ipv4_service_p;

    ipv4_service_p = IFStateGetServiceWithID(service_p->ifstate, 
					     service_p->serviceID, 
					     IS_IPV4);
    bzero(&info, sizeof(info));
    if (ipv4_service_p != NULL && ServiceIsPublished(ipv4_service_p)) {
	(void)config_method_event(ipv4_service_p, IFEventID_get_dhcp_info_e,
				  &info);
    }
    dns = DNSEntityCreateWithInfo(if_name(if_p), &info, info_v6_p);
    *capport_dict_p = CaptivePortalEntityCreateWithInfo(&info, info_v6_p);
    return (dns);
}

STATIC void
ipv6_dict_add_delegated_prefix(CFMutableDictionaryRef dict,
			       const struct in6_addr * prefix_p,
			       uint8_t prefix_length,
			       uint32_t valid_lifetime,
			       uint32_t preferred_lifetime)
{
    if (prefix_length == 0
	|| IN6_IS_ADDR_UNSPECIFIED(prefix_p)) {
	return;
    }
    my_CFDictionarySetIPv6AddressAsString(dict,
					  kSCPropNetIPv6DelegatedPrefix,
					  prefix_p);
    my_CFDictionarySetUInt64(dict,
			     kSCPropNetIPv6DelegatedPrefixLength,
			     prefix_length);
    my_CFDictionarySetUInt64(dict,
			     kSCPropNetIPv6DelegatedPrefixValidLifetime,
			     valid_lifetime);
    my_CFDictionarySetUInt64(dict,
			     kSCPropNetIPv6DelegatedPrefixPreferredLifetime,
			     preferred_lifetime);
}

PRIVATE_EXTERN void
ServicePublishSuccessIPv6(ServiceRef service_p,
			  inet6_addrinfo_t * addresses, int addresses_count,
			  const struct in6_addr * router, int router_count,
			  ipv6_info_t * info_p,
			  CFStringRef signature)
{
    CFDictionaryRef		dhcp_dict = NULL;
    CFDictionaryRef		dns_dict = NULL;
    CFDictionaryRef		pvd_dict = NULL;
    CFDictionaryRef		capport_dict = NULL;
    CFTypeRef			entities[N_PUBLISH_ENTITIES];
    CFStringRef			entities_summary;
    int				entity_count;
    const char *		extra_string;
    boolean_t			has_addresses;
    interface_t *		if_p = service_interface(service_p);
    IFStateRef			ifstate = service_ifstate(service_p);
    CFDictionaryRef		ipv4_dict = NULL;
    CFMutableDictionaryRef	ipv6_dict = NULL;
    boolean_t			perform_plat_discovery = FALSE;
    CFDictionaryRef		rank_dict = NULL;
    CFDictionaryRef		values[N_PUBLISH_ENTITIES];

    if (service_p->serviceID == NULL) {
	return;
    }
    ServiceSetIsPublished(service_p);

    if (S_scd_session == NULL) {
	/* configd is not running */
	return;
    }
    has_addresses
	= (addresses != NULL && addresses_count != 0);

    /* IPv6 */
    ipv6_dict = CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
    if (info_p != NULL && info_p->is_stateful) {
	/* DelegatedPrefix information */
	ipv6_dict_add_delegated_prefix(ipv6_dict,
				       &info_p->prefix,
				       info_p->prefix_length,
				       info_p->prefix_valid_lifetime,
				       info_p->prefix_preferred_lifetime);
    }

    /* Addresses, PrefixLength */
    if (has_addresses) {
	CFStringRef	nat64_prefix = NULL;

	if (info_p != NULL) {
	    nat64_prefix = info_p->nat64_prefix;
	    ipv4_dict = info_p->ipv4_dict;
#ifndef kSCPropNetIPv6NAT64Prefix
#define kSCPropNetIPv6NAT64Prefix	CFSTR("NAT64Prefix")
#endif /* kSCPropNetIPv6NAT64Prefix */
	    /* NAT64 Prefix */
	    if (nat64_prefix != NULL) {
		CFDictionarySetValue(ipv6_dict, kSCPropNetIPv6NAT64Prefix,
				     nat64_prefix);
	    }
	    else if (ipv4_dict == NULL) {
		/* prefix isn't already known, discover PLAT if told to do so */
		perform_plat_discovery = info_p->perform_plat_discovery;
		if (perform_plat_discovery) {
		    CFDictionarySetValue(ipv6_dict,
					 kSCPropNetIPv6PerformPLATDiscovery,
					 kCFBooleanTrue);
		}
	    }
	}
	dict_set_inet6_info(ipv6_dict, addresses, addresses_count);
	/* Router */
	if (router != NULL) {
	    my_CFDictionarySetIPv6AddressAsString(ipv6_dict,
						  kSCPropNetIPv6Router,
					      router);
	}
	/* DNS and Captive Portal */
	dns_dict = ServiceIPv6CopyMergedDNSAndCaptive(service_p,
						      info_p,
						      &capport_dict);
    }

    /* InterfaceName */
    CFDictionarySetValue(ipv6_dict, kSCPropInterfaceName,
			 ifstate->ifname);

    if ((if_flags(if_p) & IFF_LOOPBACK) == 0) {
	/* ConfirmedInterfaceName */
	CFDictionarySetValue(ipv6_dict, kSCPropConfirmedInterfaceName,
			     ifstate->ifname);
	/* NetworkSignature */
	if (signature != NULL) {
	    CFDictionarySetValue(ipv6_dict, kNetworkSignature,
				 signature);
	}
    }

    /* DHCPv6 */
    if (info_p != NULL) {
	dhcp_dict = DHCPv6InfoDictionaryCreate(info_p);
    }

    /* PvD Option and Additional Info */
    if (info_p != NULL) {
        pvd_dict = PvDEntityCreateWithInfo(info_p);
    }

    /*
     * Entity values can be NULL or not NULL.  The values are accumulated in
     * the "values" array.
     */
    entity_count = 0;
    set_entity_value(entities, values, N_PUBLISH_ENTITIES, 
		     kSCEntNetIPv6, ipv6_dict, &entity_count);
    if (ipv4_dict != NULL) {
	set_entity_value(entities, values, N_PUBLISH_ENTITIES,
			 kSCEntNetIPv4, ipv4_dict, &entity_count);
    }
    set_entity_value(entities, values, N_PUBLISH_ENTITIES,
		     kSCEntNetDNS, dns_dict, &entity_count);
    set_entity_value(entities, values, N_PUBLISH_ENTITIES,
		     kSCEntNetDHCPv6, dhcp_dict, &entity_count);
    set_entity_value(entities, values, N_PUBLISH_ENTITIES,
		     kSCEntNetCaptivePortal, capport_dict, &entity_count);
    set_entity_value(entities, values, N_PUBLISH_ENTITIES,
                     kSCEntNetPvD, pvd_dict, &entity_count);

    /*
     * If IPv4 hasn't published, and we're not publishing IPv4,
     * demote the rank to RankLast to allow a fully dual-stack
     * or NAT64 service to get priority.
     */
    if (has_addresses
	&& !service_interface_ipv4_published(service_p)
	&& ipv4_dict == NULL) {
	/* demote the service to RankLast */
	const void *	key = kSCPropNetServicePrimaryRank;
	const void *	rank = kSCValNetServicePrimaryRankLast;

	rank_dict = CFDictionaryCreate(NULL,
				       &key,
				       &rank,
				       1,
				       &kCFTypeDictionaryKeyCallBacks,
				       &kCFTypeDictionaryValueCallBacks);
    }
    set_entity_value(entities, values, N_PUBLISH_ENTITIES,
		     kServiceEntity, rank_dict, &entity_count);
    entities_summary = create_entities_summary(entities, values, entity_count);
    my_SCDynamicStoreSetService(S_scd_session,
				service_p->serviceID,
				entities, values, entity_count,
				service_p->no_publish);
    schedule_async_work();
    my_CFRelease(&ipv6_dict);
    my_CFRelease(&dns_dict);
    my_CFRelease(&dhcp_dict);
    my_CFRelease(&capport_dict);
    my_CFRelease(&rank_dict);
    my_CFRelease(&pvd_dict);

    setDisableUntilNeededNeedsAttention();
    if (ipconfig_method_routable(service_p->method)
	&& IFStateFlagsAreSet(ifstate, kIFStateFlagsFailureSymptomReported)) {
	report_address_acquisition_symptom(if_link_index(ifstate->if_p), true);
	IFStateFlagsClear(ifstate, kIFStateFlagsFailureSymptomReported);
	my_log(LOG_NOTICE,
	       "%s %s: reported address acquisition success symptom",
	       ipconfig_method_string(service_p->method),
	       if_name(ifstate->if_p));
    }
    if (ipv4_dict != NULL) {
	extra_string = " [464XLAT]";
    }
    else if (perform_plat_discovery) {
	extra_string = " [PLATDiscovery]";
    }
    else {
	extra_string = "";
    }
    my_log(LOG_NOTICE,
	   "%s %s: publish success { %@ }%s",
	   ipconfig_method_string(service_p->method),
	   if_name(ifstate->if_p),
	   entities_summary, extra_string);
    my_CFRelease(&entities_summary);
    return;
}

PRIVATE_EXTERN void
ServiceUnpublishCLAT46(ServiceRef service_p)
{
    if (service_clat46_is_active(service_p)) {
	CFTypeRef	entity = kSCEntNetIPv4;
	IFStateRef	ifstate = service_ifstate(service_p);
	CFDictionaryRef	value = NULL;

	my_log(LOG_NOTICE,
	       "%s %s: unpublish IPv4 (CLAT46)",
	       ipconfig_method_string(service_p->method),
	       if_name(ifstate->if_p));
	my_SCDynamicStoreSetService(S_scd_session,
				    service_p->serviceID,
				    &entity, &value, 1,
				    service_p->no_publish);
	schedule_async_work();
    }
    return;
}

PRIVATE_EXTERN boolean_t
ServiceCanDoIPv6OnlyPreferred(ServiceRef service_p)
{
    IFStateRef		ifstate = service_ifstate(service_p);
    boolean_t		ipv6_only_preferred = FALSE;
    dynarray_t *	list = &ifstate->services_v6;

    for (int i = 0; i < dynarray_count(list); i++) {
	ServiceRef	service_p = dynarray_element(list, i);

	if (service_p->method == ipconfig_method_automatic_v6_e
	    || service_p->method == ipconfig_method_rtadv_e) {
	    ipv6_only_preferred = TRUE;
	    break;
	}
    }
    my_log(LOG_INFO, "%s: IPv6OnlyPreferred is %spossible",
	   if_name(ifstate->if_p), ipv6_only_preferred ? "" : "not ");
    return (ipv6_only_preferred);
}

PRIVATE_EXTERN boolean_t
ServiceIsPublished(ServiceRef service_p)
{
    return (service_p->ready && service_p->status == ipconfig_status_success_e);
}

STATIC void
ServiceSetIsPublished(ServiceRef service_p)
{
    service_p->ready = TRUE;
    service_p->status = ipconfig_status_success_e;
}

PRIVATE_EXTERN void
ServiceGenerateFailureSymptom(ServiceRef service_p)
{
    IFStateRef	ifstate = service_ifstate(service_p);
    ServiceRef	routable_service_p;

    if (IFStateFlagsAreSet(ifstate, kIFStateFlagsFailureSymptomReported)) {
	my_log(LOG_INFO, "%s %s: symptom failure already reported",
	       ipconfig_method_string(service_p->method),
	       if_name(ifstate->if_p));
	return;
    }
    /* if any service is routable, don't generate a symptom */
    routable_service_p = IFStateGetFirstRoutableService(ifstate);
    if (routable_service_p != NULL) {
	my_log(LOG_INFO, "%s %s: won't report symptom: %s is active",
	       ipconfig_method_string(service_p->method),
	       if_name(ifstate->if_p),
	       ipconfig_method_string(routable_service_p->method));
	return;
    }
    if (report_address_acquisition_symptom(if_link_index(ifstate->if_p),
					   false)) {
	IFStateFlagsSet(ifstate, kIFStateFlagsFailureSymptomReported);
	my_log(LOG_NOTICE,
	       "%s %s: reported address acquisition failure symptom",
	       ipconfig_method_string(service_p->method),
	       if_name(ifstate->if_p));
    }
    return;
}

STATIC CFStringRef
ipconfig_method_to_cfstring(ipconfig_method_t method)
{
    CFStringRef		str;

    switch (method) {
        case ipconfig_method_bootp_e:
            str = kSCValNetIPv4ConfigMethodBOOTP;
            break;
        case ipconfig_method_dhcp_e:
            str = kSCValNetIPv4ConfigMethodDHCP;
            break;
        case ipconfig_method_manual_e:
            str = kSCValNetIPv4ConfigMethodManual;
            break;
        case ipconfig_method_inform_e:
            str = kSCValNetIPv4ConfigMethodINFORM;
            break;
        case ipconfig_method_linklocal_e:
            str = kSCValNetIPv4ConfigMethodLinkLocal;
            break;
        case ipconfig_method_failover_e:
            str = kSCValNetIPv4ConfigMethodFailover;
            break;
        case ipconfig_method_manual_v6_e:
            str = kSCValNetIPv6ConfigMethodManual;
            break;
        case ipconfig_method_automatic_v6_e:
            str = kSCValNetIPv6ConfigMethodAutomatic;
            break;
        case ipconfig_method_rtadv_e:
            str = kSCValNetIPv6ConfigMethodRouterAdvertisement;
            break;
        case ipconfig_method_stf_e:
            str = kSCValNetIPv6ConfigMethod6to4;
            break;
        case ipconfig_method_linklocal_v6_e:
            str = kSCValNetIPv6ConfigMethodLinkLocal;
            break;
	case ipconfig_method_dhcpv6_pd_e:
	    str = kSCValNetIPv6ConfigMethodDHCPv6PD;
	    break;
        default:
            str = CFSTR("<unknown>");
            break;
    }
    return (str);
}

STATIC CFDictionaryRef
ServiceCopySummary(ServiceRef service_p)
{
    CFStringRef			method;
    CFMutableDictionaryRef	summary;

    summary = CFDictionaryCreateMutable(NULL, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
    method = ipconfig_method_to_cfstring(service_p->method);
    CFDictionarySetValue(summary, kSCPropNetIPv4ConfigMethod, method);
    CFDictionarySetValue(summary, CFSTR("IsPublished"),
			 ServiceIsPublished(service_p)
			 ? kCFBooleanTrue : kCFBooleanFalse);
    if (service_p->is_dynamic) {
	CFDictionarySetValue(summary, CFSTR("IsDynamic"), kCFBooleanTrue);
    }
    if (service_p->status != ipconfig_status_success_e) {
	CFStringRef		status_cfstring;
	const char *		status_string;

	status_string = ipconfig_status_string(service_p->status);
	status_cfstring = CFStringCreateWithCString(NULL, status_string,
						    kCFStringEncodingASCII);
	CFDictionarySetValue(summary,
			     CFSTR("LastFailureStatus"), status_cfstring);
	CFRelease(status_cfstring);
    }
    CFDictionarySetValue(summary, CFSTR("ServiceID"), service_p->serviceID);
    if (service_p->child_serviceID != NULL) {
	CFDictionarySetValue(summary, CFSTR("ChildServiceID"),
			     service_p->child_serviceID);
    }
    else if (service_p->parent_serviceID != NULL) {
	CFDictionarySetValue(summary, CFSTR("ParentServiceID"),
			     service_p->parent_serviceID);
    }
    if (ServiceIsIPv4(service_p)) {
	struct in_addr		addr;
	inet_addrinfo_t *	info_p;
	ServiceIPv4Ref		v4_p = &service_p->u.v4;

	/* IPv4 service */
	info_p = &v4_p->info;
	if (info_p->addr.s_addr != 0) {
	    my_CFDictionarySetIPAddressAsArrayValue(summary,
						    kSCPropNetIPv4Addresses,
						    info_p->addr);
	}
	if (info_p->mask.s_addr != 0) {
	    my_CFDictionarySetIPAddressAsArrayValue(summary,
						    kSCPropNetIPv4SubnetMasks,
						    info_p->mask);
	}
	if (ServiceGetActiveIPAddress(service_p).s_addr != 0
	    && v4_p->router.iaddr.s_addr != 0) {
	    boolean_t		verified;

	    my_CFDictionarySetIPAddressAsString(summary,
						kSCPropNetIPv4Router,
						v4_p->router.iaddr);
	    verified = ((v4_p->router.flags & RIFLAGS_ARP_VERIFIED) != 0);
	    CFDictionarySetValue(summary,
				 CFSTR("RouterARPVerified"),
				 verified
				 ? kCFBooleanTrue : kCFBooleanFalse);
	}
	addr = v4_p->requested_ip.addr;
	if (addr.s_addr != 0) {
	    struct in_addr	mask;

	    my_CFDictionarySetIPAddressAsString(summary,
						CFSTR("ManualAddress"),
						addr);
	    mask = v4_p->requested_ip.mask;
	    if (mask.s_addr != 0) {
		my_CFDictionarySetIPAddressAsString(summary,
						    CFSTR("ManualSubnetMask"),
						    mask);
	    }
	}
	if (service_router_resolve_in_progress(service_p)) {
	    CFDictionarySetValue(summary, CFSTR("RouterARPInProgress"),
				 kCFBooleanTrue);
	}
	if (service_router_resolve_timed_out(service_p)) {
	    CFDictionarySetValue(summary, CFSTR("RouterARPTimedOut"),
				 kCFBooleanTrue);
	}
    }
    else {
	/* IPv6 service */
	ServiceIPv6Ref	v6_p = &service_p->u.v6;

	if (!IN6_IS_ADDR_UNSPECIFIED(&v6_p->requested_ip.addr)) {
	    my_CFDictionarySetIPv6AddressAsString(summary,
						  CFSTR("ManualAddress"),
						  &v6_p->requested_ip.addr);
	    my_CFDictionarySetUInt64(summary,
				     CFSTR("ManualPrefixLength"),
				     v6_p->requested_ip.prefix_length);
	}
    }
    if (service_p->apn_name != NULL) {
	CFDictionarySetValue(summary, kIPConfigurationServiceOptionAPNName,
			     service_p->apn_name);
    }
    (void)config_method_event(service_p, IFEventID_provide_summary_e, summary);
    return (summary);
}

PRIVATE_EXTERN void
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
	    service_clear(service_p, ipconfig_status_success_e);
	}
	else if (parent_service_p != NULL
		 && parent_service_p->u.v4.info.addr.s_addr == 0) {
	    ipconfig_status_t status;
	    
	    /* clear the information in the DynamicStore, but not the status */
	    status = parent_service_p->status;
	    service_publish_clear(parent_service_p, status);
	}
	else {
	    service_publish_clear(service_p, ipconfig_status_success_e);
	}
    }
    else {
	service_publish_clear(service_p, ipconfig_status_success_e);
    }
    service_p->ready = TRUE;
    service_p->status = status;
    my_log(LOG_NOTICE, "%s %s: status = '%s'",
	   ServiceGetMethodString(service_p),
	   if_name(service_interface(service_p)), 
	   ipconfig_status_string(status));
    if (sync == TRUE) {
	all_services_ready();
    }
    return;
}

PRIVATE_EXTERN void
service_publish_failure(ServiceRef service_p, ipconfig_status_t status)
{
    service_publish_failure_sync(service_p, status, TRUE);
    return;
}

PRIVATE_EXTERN int
service_enable_autoaddr(ServiceRef service_p)
{
    return (inet_set_autoaddr(if_name(service_interface(service_p)), 1));
}

PRIVATE_EXTERN int
service_disable_autoaddr(ServiceRef service_p)
{
    flush_routes(if_link_index(service_interface(service_p)),
		 G_ip_zeroes, G_ip_zeroes);
    return (inet_set_autoaddr(if_name(service_interface(service_p)), 0));
}

STATIC Rank
ServiceGetRank(ServiceRef service_p, CFArrayRef service_order)
{
    int i;
    CFStringRef serviceID = service_p->serviceID;

    if (IFStateFlagsAreSet(service_ifstate(service_p), kIFStateFlagsNetBoot)
	&& service_p->method == ipconfig_method_dhcp_e) {
	/* the netboot service is the best service */
	return (RANK_HIGHEST);
    }
    if (serviceID != NULL && service_order != NULL) {
	int count = CFArrayGetCount(service_order);

	for (i = 0; i < count; i++) {
	    CFStringRef s = CFArrayGetValueAtIndex(service_order, i);

	    if (CFEqual(serviceID, s)) {
		return (i);
	    }
	}
    }
    return (RANK_LOWEST);
}

STATIC Boolean
S_service_order_valid(CFArrayRef order)
{
    Boolean		good = FALSE;
    CFIndex		count;
    CFIndex		i;

    if (isA_CFArray(order) != NULL) {
	count = CFArrayGetCount(order);
    }
    else {
	count = 0;
    }
    if (count == 0) {
	goto done;
    }
    for (i = 0; i < count; i++) {
	CFStringRef	s = (CFStringRef)CFArrayGetValueAtIndex(order, i);

	if (isA_CFString(s) == NULL) {
	    goto done;
	}
    }
    good = TRUE;

 done:
    return (good);
}

static CFArrayRef
S_copy_service_order(SCDynamicStoreRef session)
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
	if (S_service_order_valid(order)) {
	    CFRetain(order);
	}
	else {
	    order = NULL;
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
PRIVATE_EXTERN ServiceRef
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

PRIVATE_EXTERN boolean_t
ServiceDADIsEnabled(ServiceRef service_p)
{
    IFStateRef			ifstate = service_ifstate(service_p);

    return (!IFStateFlagsAreSet(ifstate, kIFStateFlagsDisableDAD));
}

/*
 * Function: linklocal_service_change
 *
 * Purpose:
 *   If we're the parent of the link-local service, 
 *   send a change message to the link-local service, asking it to
 *   either allocate or not allocate an IP.
 */
PRIVATE_EXTERN void
linklocal_service_change(ServiceRef parent_service_p, boolean_t allocate)
{
    IFStateRef			ifstate = service_ifstate(parent_service_p);
    ipconfig_method_info	info;
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
    ipconfig_method_info_init(&info);
    info.method = ipconfig_method_linklocal_e;
    info.method_data.linklocal.allocate = allocate;
    (void)config_method_change(ll_service_p, &info, &needs_stop);
    return;
}

PRIVATE_EXTERN void
linklocal_set_needs_attention(void)
{
    S_linklocal_needs_attention = TRUE;
    schedule_async_work();
    return;
}

PRIVATE_EXTERN void
linklocal_set_address(ServiceRef ll_service_p, struct in_addr ll_addr)
{
    IFStateRef		ifstate = service_ifstate(ll_service_p);

    ifstate->v4_link_local = ll_addr;
    return;
}

PRIVATE_EXTERN struct in_addr
linklocal_get_address(ServiceRef ll_service_p)
{
    IFStateRef		ifstate = service_ifstate(ll_service_p);

    return (ifstate->v4_link_local);
}

/*
 * Function: linklocal_start
 * Purpose:
 *   Start a child link-local service for the given parent service.
 */
static void
linklocal_start(ServiceRef parent_service_p, boolean_t allocate)

{
    IFStateRef			ifstate = service_ifstate(parent_service_p);
    ipconfig_method_info	info;
    ServiceRef			service_p;
    ipconfig_status_t		status;

    ipconfig_method_info_init(&info);
    info.method = ipconfig_method_linklocal_e;
    info.method_data.linklocal.allocate = allocate;
    status = IFState_service_add(ifstate, NULL, &info, parent_service_p,
				 NULL, &service_p);
    if (status != ipconfig_status_success_e) {
	my_log(LOG_NOTICE,
	       "IPConfiguration: failed to start link-local service on %s, %s",
	       if_name(ifstate->if_p),
	       ipconfig_status_string(status));
    }
    return;
}

/*
 * Function: linklocal_elect
 * Purpose:
 *   Determine the "best" interface to be the parent of the IPv4LL service.
 *   Only a DHCP service can be the parent of an IPv4LL service.
 *   Once found, instantiate the service in either allocate or no-allocate mode.
 */
static void
linklocal_elect(CFArrayRef service_order)
{
    int 		i;

    my_log(LOG_DEBUG, "%s", __func__);
    for (i = 0; i < dynarray_count(&S_ifstate_list); i++) {
	Rank		best_rank = RANK_NONE;
	ServiceRef	best_service_p = NULL;
	boolean_t	election_required = TRUE;
	IFStateRef	ifstate = dynarray_element(&S_ifstate_list, i);
	int		j;
	ServiceRef 	ll_parent_p = NULL;
	ServiceRef	ll_service_p;
	Rank		rank;

	if ((if_flags(ifstate->if_p) & (IFF_LOOPBACK | IFF_POINTOPOINT)) != 0) {
	    /* no loopback or point-to-point */
	    continue;
	}
	ll_service_p = ifstate->linklocal_service_p;
	if (ll_service_p != NULL) {
	    if (ll_service_p->parent_serviceID == NULL) {
		election_required = FALSE;
		if (ll_service_p->u.v4.info.addr.s_addr != 0) {
		    best_service_p = ll_service_p;
		    best_rank = ServiceGetRank(ll_service_p, service_order);
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
			|| (service_p->status != ipconfig_status_no_server_e)
			|| service_clat46_is_active(service_p)) {
			/* service isn't ready to be a parent */
			continue;
		    }
		}
		rank = ServiceGetRank(service_p, service_order);
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
		    linklocal_start(best_service_p, allocate);
		}
	    }
	}
    }
    return;
}

PRIVATE_EXTERN int
service_set_address(ServiceRef service_p, 
		    struct in_addr addr,
		    struct in_addr mask, 
		    struct in_addr broadcast)
{
    interface_t *	if_p = service_interface(service_p);
    int			ret = 0;
    struct in_addr	netaddr;
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
	broadcast.s_addr = addr.s_addr | ~mask.s_addr;
    }
    netaddr.s_addr = addr.s_addr & mask.s_addr;
    my_log(LOG_NOTICE,
	   "%s %s: setting " IP_FORMAT " netmask " IP_FORMAT 
	   " broadcast " IP_FORMAT, 
	   ServiceGetMethodString(service_p),
	   if_name(if_p), 
	   IP_LIST(&addr), IP_LIST(&mask), IP_LIST(&broadcast));
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
	    my_log(LOG_NOTICE, "service_set_address(%s) "
		   IP_FORMAT " inet_aifaddr() failed, %s (%d)", if_name(if_p),
		   IP_LIST(&addr), strerror(errno), errno);
	}
	bzero(info_p, sizeof(*info_p));
	info_p->addr = addr;
	info_p->mask = mask;
	info_p->netaddr = netaddr;
	info_p->broadcast = broadcast;
	close(s);
    }
    service_p->u.v4.ip_assigned_time = timer_current_secs();
    service_p->u.v4.ip_conflict_count = 0;

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
	my_log(LOG_ERR,
	       "S_remove_ip_address(%s) socket() failed, %s (%d)",
	       ifname, strerror(errno), errno);
    }	
    else { 
	if (inet_difaddr(s, ifname, this_ip) < 0) {
	    ret = errno;
	    my_log(LOG_NOTICE, "%s: failed to remove IP address " IP_FORMAT
		   ", %s (%d)", ifname, IP_LIST(&this_ip),
		   strerror(errno), errno);
	}
	else {
	    my_log(LOG_NOTICE, "%s: removed IP address " IP_FORMAT,
		   ifname, IP_LIST(&this_ip));
	}
	close(s);
    }
    return (ret);
}

PRIVATE_EXTERN int
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
	    my_log(LOG_NOTICE, "%s %s: removing " IP_FORMAT,
		   ServiceGetMethodString(service_p),
		   if_name(if_p), IP_LIST(&saved_info.addr));
	    ret = S_remove_ip_address(if_name(if_p), saved_info.addr);
	}
	flush_routes(if_link_index(if_p), 
		     saved_info.addr, saved_info.broadcast);
    }
    linklocal_set_needs_attention();
    return (ret);
}

STATIC void
service_enable_clat46(ServiceRef service_p)
{
    if (!ServiceIsIPv6(service_p)) {
	return;
    }
    service_p->u.v6.enable_clat46 = TRUE;
    return;
}

PRIVATE_EXTERN boolean_t
service_clat46_is_configured(ServiceRef service_p)
{
    if (!ServiceIsIPv6(service_p)) {
	return (FALSE);
    }
    return (service_p->u.v6.enable_clat46);
}

PRIVATE_EXTERN boolean_t
service_clat46_is_active(ServiceRef service_p)
{
    IFStateRef		ifstate;

    ifstate = service_ifstate(service_p);
    return (IFStateFlagsAreSet(ifstate, kIFStateFlagsCLAT46Active));
}

PRIVATE_EXTERN void
service_clat46_set_is_active(ServiceRef service_p, boolean_t active)
{
    IFStateRef		ifstate = service_ifstate(service_p);

    if (active != IFStateFlagsAreSet(ifstate, kIFStateFlagsCLAT46Active)) {
	my_log(LOG_DEBUG, "%s: CLAT46 %sactive", if_name(ifstate->if_p),
	       active ? "" : "not ");
	IFStateFlagsSetOrClear(ifstate, kIFStateFlagsCLAT46Active, active);
	/* CLAT46 state impacts IPv4LL election */
	linklocal_set_needs_attention();
    }
}

PRIVATE_EXTERN void
service_clat46_set_is_available(ServiceRef service_p, boolean_t available)
{
    IFStateRef		ifstate = service_ifstate(service_p);

    if (available != IFStateFlagsAreSet(ifstate, kIFStateFlagsCLAT46Available)) {
	IFStateFlagsSetOrClear(ifstate, kIFStateFlagsCLAT46Available, available);
	my_log(LOG_DEBUG, "%s: CLAT46 %savailable", if_name(ifstate->if_p),
	       available ? "" : "not ");
    }
}

PRIVATE_EXTERN boolean_t
service_nat64_prefix_available(ServiceRef service_p)
{
    IFStateRef		ifstate;

    ifstate = service_ifstate(service_p);
    return (IFStateFlagsAreSet(ifstate, kIFStateFlagsNAT64PrefixAvailable));
}

PRIVATE_EXTERN boolean_t
service_plat_discovery_failed(ServiceRef service_p)
{
    IFStateRef		ifstate;

    ifstate = service_ifstate(service_p);
    return (IFStateFlagsAreSet(ifstate, kIFStateFlagsPLATDiscoveryComplete)
	    && !IFStateFlagsAreSet(ifstate, kIFStateFlagsNAT64PrefixAvailable));
}

PRIVATE_EXTERN boolean_t
service_plat_discovery_complete(ServiceRef service_p)
{
   IFStateRef		ifstate;

   ifstate = service_ifstate(service_p);
   return (IFStateFlagsAreSet(ifstate, kIFStateFlagsPLATDiscoveryComplete));
}

PRIVATE_EXTERN void
service_plat_discovery_clear(ServiceRef service_p)
{
    IFStateRef		ifstate;

    ifstate = service_ifstate(service_p);
    IFStateFlagsClear(ifstate,
		      kIFStateFlagsNAT64PrefixAvailable
		      | kIFStateFlagsPLATDiscoveryComplete);
}

STATIC void
service_set_disable_dhcpv6(ServiceRef service_p, boolean_t disable)
{
    if (!ServiceIsIPv6(service_p)) {
	return;
    }
    service_p->u.v6.disable_dhcpv6 = disable;
    return;
}

PRIVATE_EXTERN boolean_t
service_disable_dhcpv6(ServiceRef service_p)
{
    boolean_t	disable = FALSE;

    if (ServiceIsIPv6(service_p)) {
	disable = service_p->u.v6.disable_dhcpv6;
    }
    return (disable);
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

PRIVATE_EXTERN void
service_set_requested_ip_dest(ServiceRef service_p, struct in_addr dest)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref  v4_p = &service_p->u.v4;

	v4_p->requested_ip.dest = dest;
    }
    return;
}

PRIVATE_EXTERN struct in_addr
service_requested_ip_dest(ServiceRef service_p)
{
    if (ServiceIsIPv4(service_p)) {
	 ServiceIPv4Ref  v4_p = &service_p->u.v4;
	 return (v4_p->requested_ip.dest);
     }
     return (G_ip_zeroes);
}

STATIC void
service_router_flags_set_specific(ServiceRef service_p, uint32_t set_flags)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	v4_p->router.flags |= set_flags;
    }
}

STATIC void
service_router_flags_clear_specific(ServiceRef service_p, uint32_t clear_flags)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	v4_p->router.flags &= ~clear_flags;
    }
}

STATIC boolean_t
service_router_flags_are_set(ServiceRef service_p, uint32_t flags)
{
    if (ServiceIsIPv4(service_p)) {
	ServiceIPv4Ref	v4_p = &service_p->u.v4;

	return ((v4_p->router.flags & flags) == flags);
    }
    return (FALSE);
}

PRIVATE_EXTERN boolean_t
service_router_is_hwaddr_valid(ServiceRef service_p)
{
    return (service_router_flags_are_set(service_p, RIFLAGS_HWADDR_VALID));
}

PRIVATE_EXTERN void
service_router_set_hwaddr_valid(ServiceRef service_p)
{
    service_router_flags_set_specific(service_p, RIFLAGS_HWADDR_VALID);
}

PRIVATE_EXTERN void
service_router_clear_hwaddr_valid(ServiceRef service_p)
{
    service_router_flags_clear_specific(service_p, RIFLAGS_HWADDR_VALID);
}

PRIVATE_EXTERN boolean_t
service_router_is_iaddr_valid(ServiceRef service_p)
{
    return (service_router_flags_are_set(service_p, RIFLAGS_IADDR_VALID));
}

PRIVATE_EXTERN void
service_router_set_iaddr_valid(ServiceRef service_p)
{
    service_router_flags_set_specific(service_p, RIFLAGS_IADDR_VALID);
}

PRIVATE_EXTERN void
service_router_clear_iaddr_valid(ServiceRef service_p)
{
    service_router_flags_clear_specific(service_p, RIFLAGS_IADDR_VALID);
}

PRIVATE_EXTERN boolean_t
service_router_is_arp_verified(ServiceRef service_p)
{
    return (service_router_flags_are_set(service_p, RIFLAGS_ARP_VERIFIED));
}

STATIC void
service_router_clear_arp_verified(ServiceRef service_p)
{
    service_router_flags_clear_specific(service_p, RIFLAGS_ARP_VERIFIED);
}

STATIC void
service_router_set_resolve_in_progress(ServiceRef service_p)
{
    service_router_flags_set_specific(service_p, RIFLAGS_RESOLVE_IN_PROGRESS);
}

STATIC void
service_router_clear_resolve_in_progress(ServiceRef service_p)
{
    service_router_flags_clear_specific(service_p, RIFLAGS_RESOLVE_IN_PROGRESS);
}

STATIC boolean_t
service_router_resolve_in_progress(ServiceRef service_p)
{
    return (service_router_flags_are_set(service_p,
					 RIFLAGS_RESOLVE_IN_PROGRESS));
}

STATIC void
service_router_set_resolve_timed_out(ServiceRef service_p)
{
    service_router_flags_set_specific(service_p, RIFLAGS_RESOLVE_TIMED_OUT);
}

STATIC void
service_router_clear_resolve_timed_out(ServiceRef service_p)
{
    service_router_flags_clear_specific(service_p, RIFLAGS_RESOLVE_TIMED_OUT);
}

STATIC boolean_t
service_router_resolve_timed_out(ServiceRef service_p)
{
    return (service_router_flags_are_set(service_p,
					 RIFLAGS_RESOLVE_TIMED_OUT));
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
	ServiceIPv4Ref	v4_p;
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
    return (service_router_flags_are_set(service_p, RIFLAGS_ALL_VALID));
}

PRIVATE_EXTERN void
service_router_set_all_valid(ServiceRef service_p)
{
    service_router_flags_set_specific(service_p, RIFLAGS_ALL_VALID);
}

PRIVATE_EXTERN boolean_t
service_interface_no_ipv4(ServiceRef service_p)
{
    IFStateRef		ifstate = service_ifstate(service_p);

    return (dynarray_count(&ifstate->services) == 0);
}

PRIVATE_EXTERN boolean_t
service_interface_ipv4_published(ServiceRef service_p)
{
    int			count;
    IFStateRef		ifstate = service_ifstate(service_p);

    count = dynarray_count(&ifstate->services);
    for (int i = 0; i < count; i++) {
	ServiceRef	scan_p = dynarray_element(&ifstate->services, i);

	if (scan_p->method != ipconfig_method_linklocal_e
	    && ServiceIsPublished(scan_p)) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

PRIVATE_EXTERN CFStringRef
ServiceGetInterfaceName(ServiceRef service_p)
{
    IFStateRef		ifstate = service_ifstate(service_p);

    return (ifstate->ifname);
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
    return (IFStateFlagsAreSet(service_p->ifstate, kIFStateFlagsNetBoot));
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
		      int prefix_length,
		      u_int32_t flags,
		      u_int32_t valid_lifetime,
		      u_int32_t preferred_lifetime)
{
    interface_t *	if_p = service_interface(service_p);
    int			ret = 0;
    int			s;

    if (ServiceIsIPv6(service_p) == FALSE) {
	return (EINVAL);
    }
    {
	char 	ntopbuf[INET6_ADDRSTRLEN];

	my_log(LOG_INFO, "%s %s: setting %s/%d",
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
	if (inet6_aifaddr(s, if_name(if_p), addr_p, NULL, prefix_length, flags,
			  valid_lifetime, preferred_lifetime) < 0) {
	    ret = errno;
	    my_log(LOG_INFO,
		   "ServiceSetIPv6Address(%s): inet6_aifaddr() failed, %s (%d)",
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
    {
	char 	ntopbuf[INET6_ADDRSTRLEN];

	my_log(LOG_INFO, 
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

PRIVATE_EXTERN CFStringRef
ServiceGetSSID(ServiceRef service_p)
{
    IFStateRef	ifstate = service_p->ifstate;

    if (ifstate->wifi_info_p == NULL) {
	return (NULL);
    }
    return (WiFiInfoGetSSID(ifstate->wifi_info_p));
}

PRIVATE_EXTERN CFStringRef
ServiceGetNetworkID(ServiceRef service_p)
{
    IFStateRef	ifstate = service_p->ifstate;

    if (ifstate->wifi_info_p == NULL) {
	return (NULL);
    }
    return (WiFiInfoGetNetworkID(ifstate->wifi_info_p));
}

STATIC void
ServiceSetIPv4PublishNeedsAttention(ServiceRef service_p)
{
    IFStateSetIPv4PublishNeedsAttention(service_ifstate(service_p));
}

STATIC void
IPv4ServicePublishProcess(IFStateList_t * list)
{
    int		if_count;

    my_log(LOG_DEBUG, "%s", __func__);
    if_count = dynarray_count(list);
    for (int i = 0; i < if_count; i++) {
	IFStateRef	ifstate = dynarray_element(list, i);

	if (!IFStateFlagsAreSet(ifstate,
				kIFStateFlagsIPv4PublishNeedsAttention)) {
	    continue;
	}
	IFStateFlagsClear(ifstate, kIFStateFlagsIPv4PublishNeedsAttention);
	service_list_event(&ifstate->services_v6,
			   IFEventID_ipv4_publish_e,
			   NULL);
    }
}

PRIVATE_EXTERN void
ServiceSetActiveDuringSleepNeedsAttention(ServiceRef service_p)
{
    IFStateSetActiveDuringSleepNeedsAttention(service_ifstate(service_p));
    return;
}

PRIVATE_EXTERN CFStringRef
ServiceGetAPNName(ServiceRef service_p)
{
    return (service_p->apn_name);
}

PRIVATE_EXTERN boolean_t
ServiceIsCGAEnabled(ServiceRef service_p)
{
    return (!IFStateFlagsAreSet(service_ifstate(service_p),
				kIFStateFlagsDisableCGA));
}

PRIVATE_EXTERN boolean_t
ServiceIsPrivacyRequired(ServiceRef service_p, boolean_t * share_device_type_p)
{
    interface_t *	if_p = service_interface(service_p);
    boolean_t		privacy_required = FALSE;
    boolean_t		share_device_type = FALSE;

    if (if_is_wireless(if_p)) {
	IFStateRef	ifstate = service_ifstate(service_p);
	WiFiInfoRef	info_p = ifstate->wifi_info_p;

	if (info_p == NULL) {
	    /* if we don't know, assume privacy is required */
	    privacy_required = TRUE;
	    share_device_type = FALSE;
	}
	else {
	    switch (WiFiInfoGetAuthType(info_p)) {
	    case kWiFiAuthTypeNone:
	    case kWiFiAuthTypeUnknown:
		/* privacy required on open network */
		privacy_required = TRUE;
		share_device_type = FALSE;
		break;
	    default:
		/* privacy required if using private address */
		if (if_link_address_is_private(if_p)) {
		    privacy_required = TRUE;
		    share_device_type = WiFiInfoAllowSharingDeviceType(info_p);
		}
		break;
	    }
	}
    }
    if (share_device_type_p != NULL) {
	*share_device_type_p = share_device_type;
    }
    return (privacy_required);
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
	my_log(LOG_INFO, "set_loopback: inet_aifaddr() failed, %s (%d)", 
	       strerror(errno), errno);
    }
    close(s);

    /* add 127/8 route */
    if (subnet_route_add(loopback, loopback_net, loopback_mask, "lo0")
	== FALSE) {
	my_log(LOG_INFO, "set_loopback: subnet_route_add() failed, %s (%d)", 
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
	my_log(LOG_INFO, "IPConfiguration %s: removing unused " IP_FORMAT, 
	       ifname, IP_LIST(&ip));
	S_remove_ip_address(if_name(ifstate->if_p), ip);
    }
    return;
}


/**
 ** Routines for MiG interface
 **/

static dataOut_t
copy_vm_data(const void * data, size_t len,
	     mach_msg_type_number_t * option_dataCnt)
{
    vm_address_t	ret_data;
    kern_return_t	status;

    *option_dataCnt = 0;
    status = vm_allocate(mach_task_self(), &ret_data, len, TRUE);
    if (status != KERN_SUCCESS) {
	return (NULL);
    }
    bcopy(data, (char *)ret_data, len);
    *option_dataCnt = len;
    return ((dataOut_t)ret_data);
}

PRIVATE_EXTERN ipconfig_status_t
ipconfig_method_info_from_plist(CFPropertyListRef plist,
				ipconfig_method_info_t info)
{
    CFDictionaryRef	dict;
    boolean_t		is_ipv4;
    ipconfig_status_t	status = ipconfig_status_invalid_parameter_e;

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
	info->method = (is_ipv4)
	    ? ipconfig_method_none_v4_e
	    : ipconfig_method_none_v6_e;
	status = ipconfig_status_success_e;
	goto done;
    }
    if (is_ipv4) {
	status = method_info_from_dict(dict, info);
    }
    else {
	status = method_info_from_ipv6_dict(dict, info);
    }

 done:
    return (status);
}

static boolean_t
service_get_option(ServiceRef service_p, int option_code,
		   dataOut_t * option_data,
		   mach_msg_type_number_t *option_dataCnt)
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
	if (data != NULL) {
	    *option_data = copy_vm_data(data, len, option_dataCnt);
	    ret = (*option_data) != NULL;
	}
	break;
    }
    default:
	break;
    } /* switch */
    return (ret);
}

PRIVATE_EXTERN int
get_if_count()
{
    return (dynarray_count(&S_ifstate_list));
}

PRIVATE_EXTERN ipconfig_status_t
get_if_addr(const char * name, ip_address_t * addr)
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

PRIVATE_EXTERN ipconfig_status_t
get_if_option(const char * name, int option_code,
	      dataOut_t * option_data,
	      mach_msg_type_number_t *option_dataCnt)
{
    int 		i;
    boolean_t		name_match;

    *option_data = NULL;
    *option_dataCnt = 0;
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

PRIVATE_EXTERN ipconfig_status_t
get_if_packet(const char * name, dataOut_t * packet,
	      mach_msg_type_number_t *packetCnt)
{
    dhcp_info_t		dhcp_info;
    IFStateRef 		ifstate;
    int			j;

    *packet = NULL;
    *packetCnt = 0;
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
	    if (dhcp_info.pkt_size == 0) {
		break;
	    }
	    *packet = copy_vm_data(dhcp_info.pkt, dhcp_info.pkt_size,
				   packetCnt);
	    if (*packet != NULL) {
		return (ipconfig_status_success_e);
	    }
	    break;
	default:
	    break;
	}
    }
    return (ipconfig_status_not_found_e);
}

PRIVATE_EXTERN ipconfig_status_t
get_if_v6_packet(const char * name, dataOut_t * packet,
		 mach_msg_type_number_t *packetCnt)
{
    ipv6_info_t		ipv6_info;
    IFStateRef 		ifstate;
    int			j;

    *packet = NULL;
    *packetCnt = 0;
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
	    bzero(&ipv6_info, sizeof(ipv6_info));
	    (void)config_method_event(service_p, IFEventID_get_ipv6_info_e,
				      &ipv6_info);
	    if (ipv6_info.pkt_len == 0) {
		break;
	    }
	    *packet = copy_vm_data(ipv6_info.pkt, ipv6_info.pkt_len,
				   packetCnt);
	    if (*packet != NULL) {
		return (ipconfig_status_success_e);
	    }
	default:
	    break;
	}
    }
    return (ipconfig_status_not_found_e);
}

PRIVATE_EXTERN ipconfig_status_t
get_if_ra(const char * name, xmlDataOut_t * ra_data,
	  mach_msg_type_number_t * ra_data_cnt)
{
    CFDictionaryRef	dict;
    ipv6_info_t		info;
    IFStateRef 		ifstate;
    ipconfig_status_t	status = ipconfig_status_not_found_e;

    *ra_data = NULL;
    *ra_data_cnt = 0;
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, name, NULL);
    if (ifstate == NULL) {
	status = ipconfig_status_interface_does_not_exist_e;
	goto done;
    }
    for (int i = 0; i < dynarray_count(&ifstate->services_v6); i++) {
	ServiceRef service_p = dynarray_element(&ifstate->services_v6, i);

	switch (service_p->method) {
	case ipconfig_method_automatic_v6_e:
	case ipconfig_method_rtadv_e:
	    if (service_p->ready == FALSE) {
		break;
	    }
	    bzero(&info, sizeof(info));
	    (void)config_method_event(service_p, IFEventID_get_ipv6_info_e,
				      &info);
	    if (info.ra == NULL) {
		break;
	    }
	    dict = RouterAdvertisementCopyDictionary(info.ra);
	    if (dict == NULL) {
		break;
	    }
	    *ra_data = (xmlDataOut_t)
		my_CFPropertyListCreateVMData(dict, ra_data_cnt);
	    CFRelease(dict);
	    if (*ra_data != NULL) {
		status = ipconfig_status_success_e;
	    }
	default:
	    break;
	}
    }
 done:
    return (status);
}

PRIVATE_EXTERN ipconfig_status_t
copy_if_summary(const char * ifname, CFDictionaryRef * ret_summary)
{
    IFStateRef 		ifstate;
    ipconfig_status_t	status = ipconfig_status_not_found_e;
    CFDictionaryRef	summary = NULL;

    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, ifname, NULL);
    if (ifstate == NULL) {
	status = ipconfig_status_interface_does_not_exist_e;
    }
    else {
	summary = IFStateCopySummary(ifstate);
	if (summary != NULL) {
	    status = ipconfig_status_success_e;
	}
    }
    *ret_summary = summary;
    return (status);
}

PRIVATE_EXTERN ipconfig_status_t
copy_interface_list(CFArrayRef * ret_list)
{
    int 		i;
    CFMutableArrayRef	list = NULL;

    for (i = 0; i < dynarray_count(&S_ifstate_list); i++) {
        IFStateRef	ifstate = dynarray_element(&S_ifstate_list, i);

	if (dynarray_count(&ifstate->services) != 0
	    || dynarray_count(&ifstate->services_v6) != 0) {
	    if (list == NULL) {
		list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	    }
	    CFArrayAppendValue(list, ifstate->ifname);
	}
    }
    *ret_list = list;
    return (ipconfig_status_success_e);
}

PRIVATE_EXTERN ipconfig_status_t
get_dhcp_duid(dataOut_t * ret_duid, mach_msg_type_number_t * ret_duid_cnt)
{
    CFDataRef		duid = DHCPDUIDGet();
    ipconfig_status_t	status = ipconfig_status_not_found_e;

    if (duid != NULL) {
	*ret_duid = copy_vm_data(CFDataGetBytePtr(duid), CFDataGetLength(duid),
				 ret_duid_cnt);
	status = (*ret_duid != NULL) ? ipconfig_status_success_e
	    : ipconfig_status_allocation_failed_e;
    }
    else {
	status = ipconfig_status_not_found_e;
    }
    return (status);
}

PRIVATE_EXTERN ipconfig_status_t
get_dhcp_ia_id(const char * name, DHCPIAID * ia_id_p)
{
    IFStateRef 		ifstate;
    ipconfig_status_t	status;

    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, name, NULL);
    if (ifstate == NULL) {
	status = ipconfig_status_interface_does_not_exist_e;
	goto done;
    }
    /*
     * Return the DHCP IA_ID for the interface if IPv6 is configured and
     * we've established the DUID.
     */
    if (dynarray_count(&ifstate->services_v6) > 0 && DHCPDUIDGet() != NULL) {
	*ia_id_p = DHCPIAIDGet(name);
	status = ipconfig_status_success_e;
    }
    else {
	status = ipconfig_status_not_found_e;
    }
 done:
    return (status);
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
    case ipconfig_method_bootp_e:
	func = dhcp_thread;
	break;
    case ipconfig_method_failover_e:
	func =  failover_thread;
	break;
    case ipconfig_method_rtadv_e:
    case ipconfig_method_automatic_v6_e:
	if (S_configure_ipv6) {
	    func = rtadv_thread;
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
    case ipconfig_method_dhcpv6_pd_e:
	func = dhcpv6_pd_thread;
	break;
    default:
	break;
    }
    return (func);
}

static ipconfig_status_t
config_method_start(ServiceRef service_p, ipconfig_method_info_t info)
{
    IPConfigFuncRef		func;
    interface_t * 		if_p = service_interface(service_p);
    ipconfig_method_t		method = info->method;
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
    case IFT_L2VLAN:
    case IFT_IEEE8023ADLAG:
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
    return (*func)(service_p, IFEventID_start_e, &info->method_data);
}

static ipconfig_status_t
config_method_change(ServiceRef service_p,
		     ipconfig_method_info_t info,
		     boolean_t * needs_stop)
{
    change_event_data_t		change_event;
    IPConfigFuncRef		func;
    ipconfig_status_t		status;

    if (ipconfig_method_is_v6(info->method)) {
	IFStateRef	ifstate = service_ifstate(service_p);

	if (dynarray_count(&ifstate->services_v6) == 1
	    && ((info->disable_cga
		 != IFStateFlagsAreSet(ifstate, kIFStateFlagsDisableCGA))
		|| !IN6_ARE_ADDR_EQUAL(&info->ipv6_linklocal,
				       &ifstate->ipv6_linklocal))) {
	    /*
	     * If we're the only IPv6 service on the interface and the CGA
	     * disable flag or IPv6LL address have changed, we need to stop
	     * ourselves to get reconfigured.
	     */
	    my_log(LOG_NOTICE, "%s: IPv6 configuration changed, need stop",
		   if_name(ifstate->if_p));
	    *needs_stop = TRUE;
	    return (ipconfig_status_success_e);
	}
    }
    *needs_stop = FALSE;
    func = lookup_func(info->method);
    if (func == NULL) {
	return (ipconfig_status_operation_not_supported_e);
    }
    change_event.method_data = &info->method_data;
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
	       "config_method_event(%d): lookup_func(%d) failed",
	       event, method);
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
config_method_media(ServiceRef service_p, link_event_data_t link_event)
{
    /* if there's a media event, we need to re-ARP */
    service_router_clear_arp_verified(service_p);
    return (config_method_event(service_p, IFEventID_link_status_changed_e, 
				link_event));
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
config_method_renew(ServiceRef service_p, link_event_data_t link_event)
{
    /* renew forces a re-ARP too */
    service_router_clear_arp_verified(service_p);
    return (config_method_event(service_p, IFEventID_renew_e, link_event));
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

STATIC boolean_t
service_list_check_busy(dynarray_t * services_p)
{
    int		i;

    for (i = 0; i < dynarray_count(services_p); i++) {
	ServiceRef	service_p = dynarray_element(services_p, i);

	if (service_p->busy) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

STATIC Rank
service_list_get_rank(dynarray_t * list, CFArrayRef service_order,
		      boolean_t * services_ready_p)
{
    CFIndex	count;
    CFIndex	i;
    Rank	rank = RANK_NONE;
    
    *services_ready_p = FALSE;
    count = dynarray_count(list);
    for (i = 0; i < count; i++) {
	ServiceRef	service_p = dynarray_element(list, i);
	Rank		this_rank;

	if (service_p->parent_serviceID != NULL) {
	    continue;
	}
	this_rank = ServiceGetRank(service_p, service_order);
	if (this_rank < rank) {
	    rank = this_rank;
	}
	switch (service_p->method) {
	case ipconfig_method_linklocal_e:
	case ipconfig_method_linklocal_v6_e:
	    /* link-local services don't count */
	    break;
	default:
	    if (service_p->ready
		&& service_p->status == ipconfig_status_success_e) {
		*services_ready_p = TRUE;
	    }
	    break;
	}
    }
    return (rank);
}

static ServiceRef
service_list_first_routable_service(dynarray_t * list)
{
    int		i;

    for (i = 0; i < dynarray_count(list); i++) {
	ServiceRef	service_p = dynarray_element(list, i);

	if (service_p->ready
	    && service_p->status == ipconfig_status_success_e
	    && ipconfig_method_routable(service_p->method)) {
	    return (service_p);
	}
    }
    return (NULL);
}

static void
IFState_all_services_event(IFStateRef ifstate, IFEventID_t event, void * evdata)
{
    service_list_event(&ifstate->services, event, evdata);
    service_list_event(&ifstate->services_v6, event, evdata);
}

static void
IFStateList_all_services_event(IFStateList_t * list, 
			       IFEventID_t event, void * evdata)
{
    int 		i;
    int			if_count = dynarray_count(list);

    for (i = 0; i < if_count; i++) {
	IFStateRef		ifstate = dynarray_element(list, i);

	IFState_all_services_event(ifstate, event, evdata);
    }
    return;
}

static void
IFStateList_all_services_sleep(IFStateList_t * list)
{
    int 		i;
    int			if_count = dynarray_count(list);

    for (i = 0; i < if_count; i++) {
	IFStateRef	ifstate = dynarray_element(list, i);

	IFState_all_services_event(ifstate, IFEventID_sleep_e, NULL);
	my_CFRelease(&ifstate->neighbor_advert_list);
    }
    return;
}

PRIVATE_EXTERN ipconfig_status_t
set_if(const char * name, ipconfig_method_info_t info)
{
    interface_t * 	if_p = ifl_find_name(S_interfaces, name);
    IFStateRef   	ifstate;
    ipconfig_method_t	method = info->method;

    my_log(LOG_INFO, "set %s %s", name, ipconfig_method_string(method));
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
    return (IFState_service_add(ifstate, NULL, info, NULL, NULL, NULL));
}

static ipconfig_status_t
add_or_set_service(const char * name, ipconfig_method_info_t info,
		   bool add_only, ServiceID service_id,
		   CFDictionaryRef plist, pid_t pid)
{
    CFStringRef		apn_name = NULL;
    boolean_t		clear_state = FALSE;
    boolean_t		disable_dhcpv6;
    boolean_t		enable_clat46 = FALSE;
    boolean_t		enable_clat46_specified = FALSE;
    boolean_t		enable_dad = TRUE;
    interface_t * 	if_p = ifl_find_name(S_interfaces, name);
    IFStateRef   	ifstate;
    ServiceInitHandler	init_handler;
    ipconfig_method_t	method = info->method;
    pid_t		monitor_pid = -1;
    int			mtu = -1;
    boolean_t		no_publish = FALSE;
    boolean_t		perform_nud = TRUE;
    ServiceRef		service_p;
    CFStringRef		serviceID = NULL;
    ipconfig_status_t	status;

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
    service_p = IFStateGetServiceMatchingMethod(ifstate, info, FALSE);
    if (service_p != NULL) {
	boolean_t	needs_stop = FALSE;

	if (add_only) {
	    return (ipconfig_status_duplicate_service_e);
	}
	status = config_method_change(service_p, info, &needs_stop);
	if (status == ipconfig_status_success_e
	    && needs_stop == FALSE) {
	    return (ipconfig_status_success_e);
	}
	IFStateFreeService(ifstate, service_p);
    }

    /* disable DHCPv6 by default on cellular */
    disable_dhcpv6 = if_ift_type(if_p) == IFT_CELLULAR;

    /* get service options */
    if (plist != NULL) {
	CFDictionaryRef		options_dict;

	options_dict = CFDictionaryGetValue(plist, 
					    kIPConfigurationServiceOptions);
	if (isA_CFDictionary(options_dict) != NULL) {
	    CFStringRef		prop_serviceID;

	    if (S_get_plist_boolean_quiet(options_dict, 
					  _kIPConfigurationServiceOptionMonitorPID,
					  FALSE)) {
		monitor_pid = pid;
	    }
	    no_publish 
		= S_get_plist_boolean_quiet(options_dict,
					    _kIPConfigurationServiceOptionNoPublish,
					    FALSE);
	    mtu = S_get_plist_int_quiet(options_dict,
					kIPConfigurationServiceOptionMTU,
					-1);
	    perform_nud
		= S_get_plist_boolean_quiet(options_dict,
					    kIPConfigurationServiceOptionPerformNUD,
					    TRUE);
	    enable_dad
		= S_get_plist_boolean_quiet(options_dict,
					    kIPConfigurationServiceOptionEnableDAD,
					    TRUE);
	    enable_clat46
		= S_get_plist_boolean_quiet(options_dict,
					    kIPConfigurationServiceOptionEnableCLAT46,
					    FALSE);
	    if (CFDictionaryContainsKey(options_dict, 
					kIPConfigurationServiceOptionEnableCLAT46)) {
		enable_clat46_specified = TRUE;
	    }
	    disable_dhcpv6
		= !S_get_plist_boolean_quiet(options_dict,
					     kIPConfigurationServiceOptionEnableDHCPv6,
					     !disable_dhcpv6);

	    prop_serviceID
		= CFDictionaryGetValue(options_dict,
				       _kIPConfigurationServiceOptionServiceID);
	    if (isA_CFString(prop_serviceID) != NULL) {
		serviceID = CFRetain(prop_serviceID);
	    }
	    clear_state
		= S_get_plist_boolean_quiet(options_dict, 
					    kIPConfigurationServiceOptionClearState,
					    FALSE);
	    apn_name
		= CFDictionaryGetValue(options_dict,
				       kIPConfigurationServiceOptionAPNName);
	    apn_name = isA_CFString(apn_name);
	}
    }

    if (!enable_clat46_specified
	&& S_cellular_clat46_autoenable
	&& (if_ift_type(if_p) == IFT_CELLULAR)
	&& no_publish) {
	my_log(LOG_INFO, "[DEBUG] auto-enabling clat46 on %s", name);
	enable_clat46 = TRUE;
    }

    if (serviceID == NULL) {
	serviceID = my_CFUUIDStringCreate(NULL);
	if (serviceID == NULL) {
	    return (ipconfig_status_allocation_failed_e);
	}
    }

    /* add a new service */
    my_log(LOG_INFO, "%s %s %s", add_only ? "add_service" : "set_service",
	   name, ipconfig_method_string(method));
    if (mtu > 0) {
	/* set the mtu */
	my_log(LOG_INFO, "set interface %s mtu to %d", name, mtu);
	interface_set_mtu(name, mtu);
    }
    IFStateFlagsSetOrClear(ifstate, kIFStateFlagsDisablePerformNUD,
			   !perform_nud);
    IFStateFlagsSetOrClear(ifstate, kIFStateFlagsDisableDAD,
			   !enable_dad);
    IFStateFlagsClear(ifstate,
		      kIFStateFlagsNAT64PrefixAvailable
		      | kIFStateFlagsPLATDiscoveryComplete);
    if (clear_state) {
	if (ipconfig_method_is_v6(method)) {
	    IFState_detach_IPv6(ifstate);
	}
    }
    init_handler = ^(ServiceRef service_p) {
	service_p->no_publish = no_publish;
	service_p->is_dynamic = TRUE;
	service_set_disable_dhcpv6(service_p, disable_dhcpv6);
	if (enable_clat46) {
	    service_enable_clat46(service_p);
	}
    };
    status = IFState_service_add(ifstate, serviceID, info, NULL,
				 init_handler, &service_p);
    if (status == ipconfig_status_success_e) {
	if (apn_name != NULL) {
	    service_set_apn_name(service_p, apn_name);
	}
	if (monitor_pid != -1) {
	    ServiceMonitorPID(service_p, monitor_pid);
	}
	ServiceIDInitWithCFString(service_id, serviceID);
    }
    CFRelease(serviceID);
    return (status);
}

PRIVATE_EXTERN ipconfig_status_t
add_service(const char * name, ipconfig_method_info_t info,
	    ServiceID service_id,
	    CFDictionaryRef plist, pid_t pid)
{
    return (add_or_set_service(name, info, TRUE, service_id, plist, pid));
}

PRIVATE_EXTERN ipconfig_status_t
set_service(const char * name, ipconfig_method_info_t info,
	    ServiceID service_id)
{
    return (add_or_set_service(name, info, FALSE, service_id, NULL, -1));
}

STATIC ipconfig_status_t
S_remove_service(IFStateRef ifstate, ServiceRef service_p)
{
    if (service_p->is_dynamic == FALSE) {
	return (ipconfig_status_invalid_operation_e);
    }
    my_log(LOG_INFO, "remove_service %s %s", if_name(ifstate->if_p),
	   ServiceGetMethodString(service_p));

    /* remove the service */
    IFStateFreeService(ifstate, service_p);
    return (ipconfig_status_success_e);
}

STATIC IFStateRef
S_find_service_with_id(const char * ifname, CFStringRef serviceID,
		       ServiceRef * ret_service_p)
{
    IFStateRef   	ifstate;
    ServiceRef		service_p = NULL;

    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, ifname, NULL);
    if (ifstate != NULL) {
	service_p = IFStateGetServiceWithID(ifstate, serviceID, IS_IPV6);
	if (service_p == NULL) {
	    service_p = IFStateGetServiceWithID(ifstate, serviceID, IS_IPV4);
	}
	if (service_p == NULL) {
	    ifstate = NULL;
	}
    }
    *ret_service_p = service_p;
    return (ifstate);
}

STATIC ipconfig_status_t
S_remove_service_with_id_str(const char * ifname, CFStringRef serviceID)
{
    IFStateRef   	ifstate;
    ServiceRef		service_p;
    ipconfig_status_t	status;

    if (ifname != NULL) {
	ifstate = S_find_service_with_id(ifname, serviceID, &service_p);
    }
    else {
	ifstate = IFStateListGetServiceWithID(&S_ifstate_list,
					      serviceID, &service_p,
					      IS_IPV6);
	if (ifstate == NULL) {
	    ifstate = IFStateListGetServiceWithID(&S_ifstate_list,
						  serviceID, &service_p,
						  IS_IPV4);
	}
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
remove_service_with_id(const char * ifname, ServiceID service_id)
{
    CFStringRef		serviceID;
    ipconfig_status_t	status;

    serviceID = ServiceIDCreateCFString(service_id);
    if (serviceID == NULL) {
	return (ipconfig_status_allocation_failed_e);
    }
    status = S_remove_service_with_id_str(ifname, serviceID);
    CFRelease(serviceID);
    return (status);
}

PRIVATE_EXTERN ipconfig_status_t
find_service(const char * name, boolean_t exact,
	     ipconfig_method_info_t info,
	     ServiceID service_id)
{
    IFStateRef   	ifstate;
    ServiceRef		service_p;

    switch (info->method) {
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
	service_p = IFStateGetServiceWithMethod(ifstate, info, FALSE);
    }
    else {
	service_p = IFStateGetServiceMatchingMethod(ifstate, info, FALSE);
    }
    if (service_p == NULL) {
	return (ipconfig_status_no_such_service_e);
    }
    ServiceIDInitWithCFString(service_id, service_p->serviceID);
    return (ipconfig_status_success_e);
}

PRIVATE_EXTERN ipconfig_status_t
remove_service(const char * name, ipconfig_method_info_t info)
{
    IFStateRef   	ifstate;
    ServiceRef		service_p;

    switch (info->method) {
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
    service_p = IFStateGetServiceWithMethod(ifstate, info, FALSE);
    if (service_p == NULL) {
	return (ipconfig_status_no_such_service_e);
    }
    return (S_remove_service(ifstate, service_p));
}

PRIVATE_EXTERN ipconfig_status_t
refresh_service(const char * ifname, ServiceID service_id)
{
    IFStateRef		ifstate;
    CFStringRef		serviceID;
    ServiceRef		service_p;
    ipconfig_status_t	status;

    serviceID = ServiceIDCreateCFString(service_id);
    if (serviceID == NULL) {
	return (ipconfig_status_allocation_failed_e);
    }
    ifstate = S_find_service_with_id(ifname, serviceID, &service_p);
    if (ifstate == NULL) {
	status = ipconfig_status_no_such_service_e;
    }
    else {
	link_event_data		link_event;

	bzero(&link_event, sizeof(link_event));
	link_event.link_status = service_link_status(service_p);
	my_log(LOG_INFO, "%s %s: refresh",
	       if_name(ifstate->if_p),
	       ServiceGetMethodString(service_p));
	status = config_method_event(service_p, IFEventID_renew_e,
				     &link_event);
    }
    CFRelease(serviceID);
    return (status);
}

PRIVATE_EXTERN ipconfig_status_t
is_service_valid(const char * ifname, ServiceID service_id)
{
    IFStateRef		ifstate;
    CFStringRef		serviceID;
    ServiceRef		service_p;
    ipconfig_status_t	status;

    serviceID = ServiceIDCreateCFString(service_id);
    if (serviceID == NULL) {
	return (ipconfig_status_allocation_failed_e);
    }
    ifstate = S_find_service_with_id(ifname, serviceID, &service_p);
    if (ifstate == NULL) {
	status = ipconfig_status_no_such_service_e;
    }
    else {
	status = ipconfig_status_success_e;
    }
    CFRelease(serviceID);
    return (status);
}

PRIVATE_EXTERN ipconfig_status_t
forget_network(const char * name, CFStringRef ssid)
{
    IFStateRef		ifstate;

    if (ssid == NULL) {
	return (ipconfig_status_invalid_parameter_e);
    }
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, name, NULL);
    if (ifstate == NULL) {
	return (ipconfig_status_interface_does_not_exist_e);
    }
    if (!if_is_wifi_infra(ifstate->if_p)) {
	/* not wireless */
	return (ipconfig_status_invalid_parameter_e);
    }
    IFState_all_services_event(ifstate, IFEventID_forget_ssid_e, (void *)ssid);
    return (ipconfig_status_success_e);
}

static boolean_t
ipconfig_method_from_cfstring(CFStringRef m, ipconfig_method_t * method)
{
    if (isA_CFString(m) == NULL) {
	return (FALSE);
    }
    if (CFEqual(m, kSCValNetIPv4ConfigMethodBOOTP)) {
	my_log(LOG_INFO, "BOOTP is deprecated, using DHCP instead");
	*method = ipconfig_method_dhcp_e;
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

static boolean_t
plist_array_get_ip_address(CFDictionaryRef plist,
			   CFStringRef prop,
			   struct in_addr * ret_ip)
{
    CFStringRef		address;
    CFIndex		count = 0;
    boolean_t		is_valid = FALSE;
    CFArrayRef		list;

    ret_ip->s_addr = 0;
    list = CFDictionaryGetValue(plist, prop);
    if (list == NULL) {
	goto done;
    }
    if (isA_CFArray(list) == NULL) {
	my_log(LOG_NOTICE, "%@ is not an array", prop);
	goto done;
    }
    if (list != NULL) {
	count = CFArrayGetCount(list);
    }
    if (count == 0) {
	my_log(LOG_NOTICE, "%@ empty array", prop);
	goto done;
    }
    address = CFArrayGetValueAtIndex(list, 0);
    if (my_CFStringToIPAddress(address, ret_ip) == FALSE) {
	my_log(LOG_NOTICE,
	       "%@[0] is not an IP address", prop);
	goto done;
    }
    is_valid = TRUE;

 done:
    return (is_valid);
}

static ipconfig_status_t
method_info_from_dict(CFDictionaryRef dict,
		      ipconfig_method_info_t info)
{
    CFStringRef			method_cf;
    ipconfig_method_data_t	method_data = &info->method_data;
    boolean_t			status = ipconfig_status_invalid_parameter_e;

    method_cf = CFDictionaryGetValue(dict, 
				     kSCPropNetIPv4ConfigMethod);
    if (ipconfig_method_from_cfstring(method_cf, &info->method) == FALSE) {
	my_log(LOG_NOTICE,
	       "IPConfiguration: IPv4 ConfigMethod is missing/invalid");
	goto done;
    }
    if (ipconfig_method_is_manual(info->method)) {
	struct in_addr		address;
	struct in_addr		dest = { 0 };
	struct in_addr		mask = { 0 };

	if (!plist_array_get_ip_address(dict,
					kSCPropNetIPv4Addresses,
					&address)) {
	    goto done;
	}
	if (address.s_addr == 0) {
	    my_log(LOG_NOTICE,
		   "%s 0.0.0.0 is not a valid address",
		   ipconfig_method_string(info->method));
	    goto done;
	}
	if (CFDictionaryContainsKey(dict, kSCPropNetIPv4SubnetMasks)
	    && !plist_array_get_ip_address(dict, kSCPropNetIPv4SubnetMasks,
					   &mask)) {
	    my_log(LOG_NOTICE,
		   "%s: %@ invalid",
		   ipconfig_method_string(info->method),
		   kSCPropNetIPv4SubnetMasks);
	    goto done;
	}
	if (CFDictionaryContainsKey(dict, kSCPropNetIPv4DestAddresses)
	    && !plist_array_get_ip_address(dict, kSCPropNetIPv4DestAddresses,
					   &dest)) {
	    my_log(LOG_NOTICE,
		   "%s: %@ invalid",
		   ipconfig_method_string(info->method),
		   kSCPropNetIPv4DestAddresses);
	    goto done;
	}
	method_data->manual.addr = address;
	method_data->manual.mask = mask;
	method_data->manual.dest = dest;
	if (info->method == ipconfig_method_manual_e) {
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
		       "%s: %@ invalid",
		       ipconfig_method_string(info->method),
		       kSCPropNetIPv4Router);
	    }
	}
	else if (info->method == ipconfig_method_failover_e) {
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
    else if (info->method == ipconfig_method_dhcp_e) {
	char			cid[256];
	int			cid_len = 0;
	CFStringRef		client_id = NULL;

	client_id = CFDictionaryGetValue(dict, kSCPropNetIPv4DHCPClientID);
	if (isA_CFString(client_id) != NULL) {
	    cid_len = my_CFStringToCStringAndLength(client_id,
						    cid, sizeof(cid));
	    if (cid_len > 0) {
		cid_len--; /* ignore trailing nul character */
	    }
	}
	if (cid_len > 0) {
	    method_data->dhcp.client_id_len = cid_len;
	    method_data->dhcp.client_id = strdup(cid);
	    if (method_data->dhcp.client_id == NULL) {
		my_log(LOG_NOTICE,
		       "IPConfiguration: strdup DHCP client ID failed");
		status = ipconfig_status_allocation_failed_e;
		goto done;
	    }
	}
    }
    else if (info->method == ipconfig_method_linklocal_e) {
	    method_data->linklocal.allocate = LINKLOCAL_ALLOCATE;
    }
    status = ipconfig_status_success_e;

 done:
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
    else if (CFEqual(m, kSCValNetIPv6ConfigMethodDHCPv6PD)) {
	*method = ipconfig_method_dhcpv6_pd_e;
    }
    else {
	return (FALSE);
    }
    return (TRUE);
}

static ipconfig_status_t
method_info_from_ipv6_dict(CFDictionaryRef dict,
			   ipconfig_method_info_t info)
{
    struct in6_addr		ipv6_ll;
    CFStringRef			ipv6_ll_cf;
    CFStringRef			method_cf;
    ipconfig_method_data_t	method_data = &info->method_data;
    boolean_t			status = ipconfig_status_invalid_parameter_e;

    ipv6_ll_cf = CFDictionaryGetValue(dict, kSCPropNetIPv6LinkLocalAddress);
    if (my_CFStringToIPv6Address(ipv6_ll_cf, &ipv6_ll)
	&& IN6_IS_ADDR_LINKLOCAL(&ipv6_ll)) {
	info->ipv6_linklocal = ipv6_ll;
    }
    if (S_get_plist_int_quiet(dict, kSCPropNetIPv6EnableCGA, 1) == 0) {
	info->disable_cga = TRUE;
    }
    method_cf = CFDictionaryGetValue(dict, 
				     kSCPropNetIPv6ConfigMethod);
    if (ipconfig_method_from_cfstring_ipv6(method_cf, &info->method) == FALSE) {
	my_log(LOG_NOTICE,
	       "IPConfiguration: IPv6 ConfigMethod is missing/invalid");
	goto done;
    }
    if (info->method == ipconfig_method_manual_v6_e) {
	struct in6_addr		address;
	CFArrayRef		addresses;
	CFStringRef		address_cf;
	CFIndex			count = 0;
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
	    my_log(LOG_NOTICE,
		   "IPConfiguration: %s Addresses missing/invalid\n",
		   ipconfig_method_string(info->method));
	    goto done;
	}
	address_cf = CFArrayGetValueAtIndex(addresses, 0);
	if (my_CFStringToIPv6Address(address_cf, &address) == FALSE) {
	    my_log(LOG_NOTICE,
		   "IPConfiguration: %s Addresses invalid",
		   ipconfig_method_string(info->method));
	    goto done;
	}
	if (prefixes != NULL) {
	    if (count != CFArrayGetCount(prefixes)) {
		my_log(LOG_NOTICE,
		       "IPConfiguration: "
		       "%s Addresses/PrefixLength are different sizes",
		       ipconfig_method_string(info->method));
		goto done;
	    }
	    prefix_cf = CFArrayGetValueAtIndex(prefixes, 0);
	    if (isA_CFNumber(prefix_cf) == NULL
		|| (CFNumberGetValue(prefix_cf, kCFNumberIntType, &prefix)
		    == FALSE)) {
		my_log(LOG_NOTICE, "IPConfiguration: %s PrefixLength invalid",
		       ipconfig_method_string(info->method));
		goto done;
	    }
	}
	if (count > 1) {
	    my_log(LOG_NOTICE,
		   "IPConfiguration: %s "
		   "multiple addresses specified - ignoring all but first",
		   ipconfig_method_string(info->method));
	}
	method_data->manual_v6.addr = address;
	method_data->manual_v6.prefix_length = prefix;
    }
    else if (info->method == ipconfig_method_stf_e) {
	CFStringRef	relay_cf;

	relay_cf = CFDictionaryGetValue(dict, kSCPropNetIPv66to4Relay);
	if (relay_cf != NULL) {
	    char		buf[256];
	    int			len;
	    address_type_t	relay_addr_type;
	    struct in_addr	relay_ip;
	    struct in6_addr	relay_ipv6;

	    if (isA_CFString(relay_cf) == NULL) {
		my_log(LOG_NOTICE, "IPConfiguration: %s 6to4 Relay invalid",
		       ipconfig_method_string(info->method));
		goto done;
	    }
	    len = my_CFStringToCStringAndLength(relay_cf, buf, sizeof(buf));
	    if (len == 0) {
		my_log(LOG_NOTICE, "IPConfiguration: %s 6to4 Relay empty",
		       ipconfig_method_string(info->method));
		goto done;
	    }
	    if (inet_aton(buf, &relay_ip) == 1) {
		relay_addr_type = address_type_ipv4_e;
	    }
	    else if (inet_pton(AF_INET6, buf, &relay_ipv6) == 1) {
		relay_addr_type = address_type_ipv6_e;
	    }
	    else {
		relay_addr_type = address_type_dns_e;
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
		method_data->stf.relay_addr.dns = strdup(buf);
		if (method_data->stf.relay_addr.dns == NULL) {
		    my_log(LOG_NOTICE,
			   "IPConfiguration: malloc relay dns address failed");
		    status = ipconfig_status_allocation_failed_e;
		    goto done;
		}
		break;
	    }
	}
    }
    else if (info->method == ipconfig_method_dhcpv6_pd_e) {
	CFStringRef				prefix;
	CFNumberRef				prefix_length;
	ipconfig_method_data_dhcpv6_pd_t 	dhcpv6_pd;

	dhcpv6_pd = &method_data->dhcpv6_pd;
	prefix  = CFDictionaryGetValue(dict, kSCPropNetIPv6RequestedPrefix);
	if (prefix != NULL) {
	    char 	ntopbuf[INET6_ADDRSTRLEN];

	    if (isA_CFString(prefix) == NULL) {
		my_log(LOG_NOTICE,
		       "%s: %@ not a string", __func__,
		       kSCPropNetIPv6RequestedPrefix);
		goto done;
	    }
	    if (!my_CFStringToIPv6Address(prefix,
					  &dhcpv6_pd->requested_prefix)) {
		my_log(LOG_NOTICE,
		       "%s: %@ not an IPv6 address", __func__,
		       kSCPropNetIPv6RequestedPrefix);
		goto done;
	    }
	    inet_ntop(AF_INET6, &dhcpv6_pd->requested_prefix,
		      ntopbuf, sizeof(ntopbuf));
	}
	prefix_length
	    = CFDictionaryGetValue(dict,
				   kSCPropNetIPv6RequestedPrefixLength);
	if (prefix_length != NULL) {
	    uint32_t	len;

	    if (isA_CFNumber(prefix_length) == NULL) {
		my_log(LOG_NOTICE,
		       "%s: %@ not a number", __func__,
		       kSCPropNetIPv6RequestedPrefixLength);
		goto done;
	    }
	    if (!my_CFTypeToNumber(prefix_length, &len)) {
		my_log(LOG_NOTICE,
		       "%s: %@ invalid number", __func__,
		       kSCPropNetIPv6RequestedPrefixLength);
		goto done;
	    }
	    if (len > 128) {
		my_log(LOG_NOTICE,
		       "%s: %@ %d > 128", __func__,
		       kSCPropNetIPv6RequestedPrefixLength, len);
		goto done;
	    }
	    dhcpv6_pd->requested_prefix_length = len;
	}
    }
    status = ipconfig_status_success_e;

 done:
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
    CFIndex 	count;
    int		i;

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
		       CFStringRef type, CFStringRef ifn_cf,
		       CFBooleanRef disable_until_needed)
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
    if (disable_until_needed != NULL) {
	CFDictionarySetValue(service_dict, 
			     k_DisableUntilNeeded,
			     disable_until_needed);
    }
    return (service_dict);
}

static CFDictionaryRef
copy_ipv6_service_dict(CFDictionaryRef values, CFStringRef serviceID,
		       CFStringRef type, CFStringRef ifn_cf,
		       CFBooleanRef disable_until_needed)

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
    if (disable_until_needed != NULL) {
	CFDictionarySetValue(service_dict,
			     k_DisableUntilNeeded,
			     disable_until_needed);
    }
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
    CFIndex		count;
    int			i;
    const void * *	keys;
    CFMutableArrayRef	list = NULL;
    CFIndex		list_count;

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

static CFBooleanRef
S_get_disable_until_needed(CFDictionaryRef values, CFStringRef ifname)
{
    CFBooleanRef	disable_until_needed = NULL;
    CFDictionaryRef	if_dict;
    CFStringRef		key;

    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainSetup,
							ifname,
							NULL);
    if_dict = CFDictionaryGetValue(values, key);
    CFRelease(key);
    if (isA_CFDictionary(if_dict) != NULL) {
	CFNumberRef	num;

	num = CFDictionaryGetValue(if_dict,
				   kSCPropNetInterfaceDisableUntilNeeded);
	if (isA_CFNumber(num) != NULL) {
	    int		disable;
	
	    if (CFNumberGetValue(num, 
				 kCFNumberIntType, &disable)) {
		disable_until_needed
		    = (disable == 0) ? kCFBooleanFalse : kCFBooleanTrue;
	    }
	}
    }
    return (disable_until_needed);
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
    CFIndex			service_IDs_count;
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

    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainSetup,
							kSCCompAnyRegex,
							NULL);
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
	CFBooleanRef		disable_until_needed = NULL;
	CFDictionaryRef		if_dict;
	CFStringRef 		ifname;
	CFStringRef 		key;
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
	    my_log(LOG_NOTICE,
		   "IPConfiguration: Interface Type missing/invalid"
		   "\nInterface = %@", if_dict);
	    continue;
	}
	ifname = CFDictionaryGetValue(if_dict, kSCPropNetInterfaceDeviceName);
	if (isA_CFString(ifname) == NULL) {
	    continue;
	}

	/* check whether DisableUntilNeeded is set */
	disable_until_needed = S_get_disable_until_needed(values, ifname);

	/* get IPv4 service configuration */
	service_dict = copy_ipv4_service_dict(values, serviceID,
					      type, ifname,
					      disable_until_needed);
	if (service_dict != NULL) {
	    CFArrayAppendValue(all_services, service_dict);
	    CFRelease(service_dict);
	}
	/* get IPv6 service configuration */
	service_dict = copy_ipv6_service_dict(values, serviceID,
					      type, ifname,
					      disable_until_needed);
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
    CFIndex		count;
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
    CFIndex		count;
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
    ipconfig_method_info	info;
} ServiceConfig, * ServiceConfigRef;

typedef struct {
    ServiceConfigRef		list;
    int				count;
    boolean_t			is_ipv4;
    CFBooleanRef		disable_until_needed;
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
	ipconfig_method_info_free(&scan->info);
    }
    free(scl->list);
    scl->list = NULL;
    return;
}

static ServiceConfigRef
ServiceConfigListLookupMethod(ServiceConfigListRef scl,
			      ipconfig_method_info_t info)
{
    int 		i;
    ServiceConfigRef	scan;

    switch (info->method) {
    case ipconfig_method_dhcpv6_pd_e:
    case ipconfig_method_stf_e:
    case ipconfig_method_linklocal_e:
    case ipconfig_method_linklocal_v6_e:
	for (i = 0, scan = scl->list; i < scl->count; i++, scan++) {
	    if (info->method == scan->info.method) {
		return (scan);
	    }
	}
	break;
    case ipconfig_method_rtadv_e:
    case ipconfig_method_automatic_v6_e:
	for (i = 0, scan = scl->list; i < scl->count; i++, scan++) {
	    switch (scan->info.method) {
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
	    if (ipconfig_method_is_dhcp_or_bootp(scan->info.method))
		return (scan);
	}
	break;
    case ipconfig_method_failover_e:
    case ipconfig_method_manual_e:
    case ipconfig_method_inform_e:
	for (i = 0, scan = scl->list; i < scl->count; i++, scan++) {
	    if (ipconfig_method_is_manual(scan->info.method)
		&& (info->method_data.manual.addr.s_addr
		    == scan->info.method_data.manual.addr.s_addr)) {
		return (scan);
	    }
	}
	break;
    case ipconfig_method_manual_v6_e:
	for (i = 0, scan = scl->list; i < scl->count; i++, scan++) {
	    if (scan->info.method != ipconfig_method_manual_v6_e) {
		continue;
	    }
	    if (IN6_ARE_ADDR_EQUAL(&info->method_data.manual_v6.addr,
				   &scan->info.method_data.manual_v6.addr)) {
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
find_dynamic_service(const char * ifname, ipconfig_method_info_t info)
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
    return (IFStateGetServiceMatchingMethod(ifstate, info, TRUE));
}

static boolean_t
ServiceConfigListInit(ServiceConfigListRef scl, boolean_t is_ipv4,
		      CFArrayRef all_services, const char * ifname)
{
    int			i;
    CFArrayRef 		if_service_list;
    CFIndex		if_service_count;
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
    scl->count = 0;
    for (i = 0; i < if_service_count; i++) {
	boolean_t		duplicate_config = FALSE;
	boolean_t		duplicate_dynamic = FALSE;
	ipconfig_method_info	info;
	CFDictionaryRef		service_dict;
	CFStringRef		serviceID;

	ipconfig_method_info_init(&info);
	service_dict = CFArrayGetValueAtIndex(if_service_list, i);
	if (is_ipv4) {
	    if (method_info_from_dict(service_dict, &info)
		!= ipconfig_status_success_e) {
		continue;
	    }
	}
	else {
	    if (method_info_from_ipv6_dict(service_dict, &info)
		!= ipconfig_status_success_e) {
		continue;
	    }
	}
	duplicate_config
	    = (ServiceConfigListLookupMethod(scl, &info) != NULL);
	if (duplicate_config == FALSE) {
	    duplicate_dynamic = (find_dynamic_service(ifname, &info) != NULL);
	}
	if (duplicate_config || duplicate_dynamic) {
	    my_log(LOG_NOTICE, "%s: %s %s",
		   ifname, ipconfig_method_string(info.method),
		   duplicate_config 
		   ? "duplicate configured service" 
		   : "configured service conflicts with dynamic service");
	    ipconfig_method_info_free(&info);
	    continue;
	}
	scl->disable_until_needed
	    = CFDictionaryGetValue(service_dict, k_DisableUntilNeeded);
	serviceID = CFDictionaryGetValue(service_dict, PROP_SERVICEID);
	scl->list[scl->count].serviceID = CFRetain(serviceID);
	scl->list[scl->count].info = info;
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
    CFIndex		inactive_list_count;
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

	if (service_p->method == config->info.method) {
	    status = config_method_change(service_p, &config->info,
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
    return (IFState_service_add(ifstate, serviceID, &config->info,
				NULL, NULL, NULL));
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

	IFStateSetDisableUntilNeededRequested(ifstate,
					      scl.disable_until_needed);
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

    /* notify when the NAT64 [prefix] key of any interface changes */
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							kSCCompAnyRegex,
							kSCEntNetNAT64);
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

    /* notify when ActiveDuringSleepRequested changes */
    key = ActiveDuringSleepRequestedKeyCopy(kSCCompAnyRegex);
    CFArrayAppendValue(patterns, key);
    CFRelease(key);

    /* notify when DisableUntilNeeded property on any interface changes */
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainSetup,
							kSCCompAnyRegex,
							NULL);
    CFArrayAppendValue(patterns, key);
    CFRelease(key);

    /* notify when IPv6RouterExpired property on any interface changes */
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							kSCCompAnyRegex,
							kSCEntNetIPv6RouterExpired);
    CFArrayAppendValue(patterns, key);
    CFRelease(key);

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

    SCDynamicStoreSetDispatchQueue(session, IPConfigurationAgentQueue());

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
	my_log(LOG_NOTICE, "IPConfiguration: ifl_init failed");
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
	interface_t *	if_p;
	IFStateRef	ifstate = dynarray_element(&S_ifstate_list, i);
	boolean_t	need_remove = FALSE;
	
	if_p = ifl_find_name(S_interfaces, if_name(ifstate->if_p));
	if (if_p == NULL) {
	    need_remove = TRUE;
	}
	else if (if_link_index(if_p) != if_link_index(ifstate->if_p)) {
	    my_log(LOG_NOTICE, "%s: index changed from %d => %d",
		   if_name(if_p), if_link_index(ifstate->if_p),
		   if_link_index(if_p));
	    need_remove = TRUE;
	}
	if (need_remove) {
	    names[names_count++] = if_name(ifstate->if_p);
	}
    }
    for (i = 0; i < names_count; i++) {
	IFStateList_ifstate_free(&S_ifstate_list, names[i]);
    }
    free(names);
    return;
}

/*
 * perform_async_work
 * - handles "async" work signaled from some context to avoid
 *   re-entrancy issues
 */
static dispatch_source_t	S_async_work_source;

STATIC void
schedule_async_work(void)
{
    dispatch_source_merge_data(S_async_work_source, 1);
}

STATIC void
perform_async_work(void)
{
    if (S_scd_session == NULL) {
	return;
    }
    my_log(LOG_DEBUG, "%s", __func__);
    if (S_linklocal_needs_attention
	|| S_disable_until_needed_needs_attention) {
	CFArrayRef		service_order = NULL;

	service_order = S_copy_service_order(S_scd_session);
	if (S_linklocal_needs_attention) {
	    S_linklocal_needs_attention = FALSE;
	    linklocal_elect(service_order);
	}
	if (S_disable_until_needed_needs_attention) {
	    S_disable_until_needed_needs_attention = FALSE;
	    DisableUntilNeededProcess(&S_ifstate_list, service_order);
	}
	my_CFRelease(&service_order);
    }
    if (S_active_during_sleep_needs_attention) {
	S_active_during_sleep_needs_attention = FALSE;
	ActiveDuringSleepProcess(&S_ifstate_list);
    }
    if (S_ipv4_publish_needs_attention) {
	S_ipv4_publish_needs_attention = FALSE;
	IPv4ServicePublishProcess(&S_ifstate_list);
    }
    my_SCDynamicStorePublish(S_scd_session);
    return;
}

STATIC void
IFState_update_link_event_data(IFStateRef ifstate, link_event_data_t link_event,
			       link_status_t * link_status_p)
{
    interface_t *	if_p = ifstate->if_p;

    bzero(link_event, sizeof(*link_event));
    link_event->link_status = *link_status_p;
    if (if_is_wifi_infra(if_p)) {
	WiFiInfoRef		info_p;

	info_p = S_copy_wifi_info(ifstate->ifname);
	if (info_p != NULL) {
	    WiFiInfoComparisonResult	result;

	    result = WiFiInfoCompare(ifstate->wifi_info_p, info_p);
	    switch (result) {
	    case kWiFiInfoComparisonResultNetworkChanged:
		/* we're on a different network */
		link_event->info = kLinkInfoNetworkChanged;
		my_log(LOG_NOTICE, "%s: Wi-Fi switched networks",
		       if_name(if_p));
		break;
	    case kWiFiInfoComparisonResultBSSIDChanged:
		/* just the BSSID changed, we roamed */
		if (!if_is_awdl(if_p)) {
		    my_log(LOG_NOTICE, "%s: Wi-Fi roam", if_name(if_p));
		}
		link_event->info = kLinkInfoBSSIDChanged;
		break;
	    default:
		break;
	    }
	}
	/*
	 * If the wifi information is non-NULL, update it now.
	 * If the wifi information is NULL, only clear it if the
	 * link status is inactive (27755476)
	 */
	if (info_p != NULL
	    || link_status_is_inactive(link_status_p)) {
	    IFState_set_wifi_info(ifstate, info_p);
	}
	my_CFRelease(&info_p);
    }
    return;
}

STATIC void
S_ifstate_process_wake(IFStateRef ifstate)
{
    boolean_t		force_attach = FALSE;
    interface_t	*	if_p = ifstate->if_p;
    link_event_data	link_event;
    link_status_t	link_status;

    link_status = if_get_link_status(if_p);
    if (link_status.valid 
	&& link_status.active == FALSE
	&& link_status.wake_on_same_network) {
	my_log(LOG_INFO, "%s: wake on same network (link inactive)",
	       if_name(if_p));
	return;
    }
    if (IFStateFlagsAreSet(ifstate, kIFStateFlagsLinkTimerSuppressed)) {
	IFStateFlagsClear(ifstate,  kIFStateFlagsLinkTimerSuppressed);
	my_log(LOG_INFO, "%s: processing link timer expired at wake",
	       if_name(if_p));
	process_link_timer_expired(ifstate);
	force_attach = TRUE;
    }
    ifstate->wake_generation = S_wake_generation;
    my_log(LOG_INFO, "%s: Wake", if_name(if_p));

    /* check for link changes */
    IFState_update_link_event_data(ifstate, &link_event,
				   &link_status);

    /* if the interface is marked as disabled, ignore the wake */
    if (ifstate->disable_until_needed.interface_disabled) {
	my_log(LOG_INFO,
	       "%s: ignoring wake (interface is disabled)", if_name(if_p));
    }
    else {
	if (dynarray_count(&ifstate->services) > 0) {
	    /* attach IPv4 in case the interface went away during sleep */
	    inet_attach_interface(if_name(if_p), TRUE);
	    service_list_event(&ifstate->services, IFEventID_wake_e,
			       &link_event);
	}
	if (dynarray_count(&ifstate->services_v6) > 0) {
	    /* if IPv6LL was stopped, or IPv6 isn't attached, initialize now */
	    if (force_attach || !inet6_is_attached(if_name(if_p))) {
		(void)IFState_attach_IPv6(ifstate, TRUE);
	    }

	    /* update our neighbor advert list */
	    if (ifstate->neighbor_advert_list != NULL) {
		CFRelease(ifstate->neighbor_advert_list);
	    }
	    ifstate->neighbor_advert_list 
		= S_copy_neighbor_advert_list(S_scd_session, ifstate->ifname);
	    service_list_event(&ifstate->services_v6, IFEventID_wake_e,
			       &link_event);
	}
    }
    return;
}

STATIC void
S_deliver_wake_event(void)
{
    int 		i;
    int			if_count = dynarray_count(&S_ifstate_list);

    for (i = 0; i < if_count; i++) {
	IFStateRef		ifstate = dynarray_element(&S_ifstate_list, i);

	if (ifstate->wake_generation == S_wake_generation) {
	    /* we've already seen this wake via link status event */
	    my_log(LOG_INFO, "%s: ignoring wake (already processed)",
		   if_name(ifstate->if_p));
	    continue;
	}
	S_ifstate_process_wake(ifstate);
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
	if (S_awake == FALSE) {
	    /* already asleep (should not happen) */
	    break;
	}
	my_log(LOG_INFO, "IPConfiguration: Sleep");
	S_awake = FALSE;
	IFStateList_all_services_sleep(&S_ifstate_list);
	break;

    case kIOMessageSystemWillPowerOn:
	if (S_awake) {
	    /* already awake (should not happen) */
	    break;
	}
	my_log(LOG_INFO, "IPConfiguration: Wake");
	S_awake = TRUE;
	S_wake_time = timer_current_secs();
	S_wake_generation++;
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
    IONotificationPortRef 	port;
    io_connect_t 		power_connection;

    power_connection = IORegisterForSystemPower(NULL, &port,
						power_changed, &obj);
    if (power_connection != 0) {
	IONotificationPortSetDispatchQueue(port,
					   IPConfigurationAgentQueue());
    }
    return (power_connection);
}

#if TARGET_OS_OSX
#define POWER_INTEREST	(kIOPMEarlyWakeNotification	\
			 | kIOPMCapabilityNetwork	\
			 | kIOPMCapabilityCPU)

void 
new_power_changed(void * param, 
		  IOPMConnection connection,
		  IOPMConnectionMessageToken token, 
		  IOPMSystemPowerStateCapabilities capabilities)
{
    IOReturn                ret;

    if ((capabilities & kIOPMCapabilityCPU) != 0) {
	if (S_awake == FALSE) {
	    S_wake_generation++;
	    S_wake_time = timer_current_secs();
	}
	S_awake = TRUE;
	if ((capabilities & kIOPMEarlyWakeNotification) != 0) {
	    my_log(LOG_INFO, "IPConfiguration: Early Wake");
	}
	else if (S_wake_event_sent == FALSE) {
	    my_log(LOG_INFO, "IPConfiguration: Wake");
	    S_deliver_wake_event();
	    S_wake_event_sent = TRUE;
	}
    }
    else {
	/* sleep */
	S_awake = FALSE;
	S_wake_event_sent = FALSE;
	my_log(LOG_INFO, "IPConfiguration: Sleep");
	IFStateList_all_services_sleep(&S_ifstate_list);

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
			       POWER_INTEREST,
			       &connection);
    if (ret != kIOReturnSuccess) {
	my_log(LOG_NOTICE,
	       "IPConfiguration: IOPMConnectionCreate failed, 0x%08x", ret);
	goto failed;
    }
    ret = IOPMConnectionSetNotification(connection, NULL,
					new_power_changed);
    
    if (ret != kIOReturnSuccess) {
	my_log(LOG_NOTICE, "IPConfiguration:"
	       "IOPMConnectionSetNotification failed, 0x%08x", ret);
	goto failed;
    }

    IOPMConnectionSetDispatchQueue(connection, IPConfigurationAgentQueue());
    return;

 failed:
    if (connection != NULL) {
	IOPMConnectionRelease(connection);
    }
    return;
}

#endif /* TARGET_OS_OSX */

static void
start_initialization(SCDynamicStoreRef session)
{
    dispatch_block_t	handler;
    dispatch_source_t	source;

    /* initialize the "observer" */
    source = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_ADD,
				    0,
				    0,
				    IPConfigurationAgentQueue());
    handler = ^{
	perform_async_work();
    };
    S_async_work_source = source;
    dispatch_source_set_event_handler(source, handler);
    dispatch_activate(source);

    /* initialize strings */
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

    /* install run-time notifiers */
    notifier_init(session);

    (void)update_interface_list();

    (void)S_netboot_init();

    configure_from_cache(session);

    /* register for sleep/wake */
#if TARGET_OS_OSX
    if (S_use_maintenance_wake) {
	new_power_notification_init();
    }
    else {
	S_power_connection = power_notification_init();
    }
#else /* TARGET_OS_OSX */
    S_power_connection = power_notification_init();
#endif /* TARGET_OS_OSX */
    return;
}

static void
link_refresh(SCDynamicStoreRef session, CFStringRef cache_key)
{
    CFStringRef			ifn_cf = NULL;
    IFStateRef   		ifstate;
    int 			j;
    link_event_data		link_event;
    
    if (CFStringHasPrefix(cache_key, S_state_interface_prefix) == FALSE) {
	return;
    }
    /* State:/Network/Interface/<ifname>/RefreshConfiguration */
    ifn_cf = my_CFStringCopyComponent(cache_key, CFSTR("/"), 3);
    if (ifn_cf == NULL) {
	return;
    }
    ifstate = IFStateListGetIFState(&S_ifstate_list, ifn_cf, NULL);
    if (ifstate == NULL || IFStateFlagsAreSet(ifstate, kIFStateFlagsNetBoot)) {
	/* don't propagate media status events for netboot interface */
	goto done;
    }
    bzero(&link_event, sizeof(link_event));
    link_event.link_status = if_get_link_status(ifstate->if_p);
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	ServiceRef	service_p = dynarray_element(&ifstate->services, j);

	config_method_renew(service_p, &link_event);
    }
    service_list_event(&ifstate->services_v6, IFEventID_renew_e, &link_event);

 done:
    my_CFRelease(&ifn_cf);
    return;
}

static void
handle_ipv6_linklocal_duplicated(IFStateRef ifstate,
				 inet6_addrlist_t * addr_list_p)
{
    boolean_t		link_active;
    link_status_t	link_status;
    inet6_addrinfo_t *	linklocal;
    char 		ntopbuf[INET6_ADDRSTRLEN];

    if (dynarray_count(&ifstate->services_v6) == 0) {
	/* no services configured, we don't care */
	goto done;
    }
    linklocal = inet6_addrlist_get_linklocal(addr_list_p);
    if (linklocal == NULL) {
	/* no linklocal address assigned, nothing to do */
	goto done;
    }
    if ((linklocal->addr_flags & IN6_IFF_DUPLICATED) == 0) {
	/* not duplicated */
	goto done;
    }
    inet_ntop(AF_INET6, linklocal, ntopbuf, sizeof(ntopbuf));
    my_log(LOG_NOTICE, "%s: IPv6LL address %s is duplicated",
	   if_name(ifstate->if_p), ntopbuf);
    if (IFStateFlagsAreSet(ifstate, kIFStateFlagsDisableCGA)) {
	/* nothing to be done about this */
	goto done;
    }
    if (ifstate->v6_ll_collision_count == IPV6_LL_MAX_COLLISION_COUNT) {
	my_log(LOG_NOTICE, "%s: IPv6LL too many collisions (%d allowed)",
	       if_name(ifstate->if_p), IPV6_LL_MAX_COLLISION_COUNT);
	goto done;
    }
    ifstate->v6_ll_collision_count++;
    my_log(LOG_NOTICE, "%s: IPv6LL stop/start (collision count %d)",
	   if_name(ifstate->if_p), ifstate->v6_ll_collision_count);
    (void)inet6_linklocal_stop(if_name(ifstate->if_p));
    link_status = if_get_link_status(ifstate->if_p);
    link_active = (!link_status.valid || link_status.active);
    (void)IFState_attach_IPv6(ifstate, link_active);

 done:
    return;
}

static void
ipv6_interface_address_changed(SCDynamicStoreRef session,
			       CFStringRef cache_key)
{
    inet6_addrlist_t 	addr_list;
    CFStringRef		ifn_cf;
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
    ifstate = IFStateListGetIFState(&S_ifstate_list, ifn_cf, NULL);
    if (ifstate == NULL) {
	/* not tracking this event */
	goto done;
    }
    /* get the addresses from the interface and deliver the event */
    inet6_addrlist_copy(&addr_list, if_link_index(ifstate->if_p));
    if (G_IPConfiguration_verbose) {
	CFStringRef		str;

	str = inet6_addrlist_copy_description(&addr_list);
	my_log(~LOG_INFO, "%@: IPv6 address list = %@", ifn_cf, str);
	CFRelease(str);
    }
    handle_ipv6_linklocal_duplicated(ifstate, &addr_list);
    service_list_event(&ifstate->services_v6,
		       IFEventID_ipv6_address_changed_e,
		       &addr_list);
    /* send neighbor advertisements if necessary */
    S_process_neighbor_adverts(ifstate, &addr_list);

    inet6_addrlist_free(&addr_list);

 done:
    my_CFRelease(&ifn_cf);
    return;
}

static void
ipv6_router_expired(SCDynamicStoreRef session, CFStringRef cache_key)
{
    CFStringRef		ifn_cf;
    IFStateRef   	ifstate;

    if (CFStringHasPrefix(cache_key, S_state_interface_prefix) == FALSE) {
	return;
    }

    /* figure out which interface this belongs to and deliver the event */

    /* State:/Network/Interface/<ifname>/IPv6RouterExpired */
    ifn_cf = my_CFStringCopyComponent(cache_key, CFSTR("/"), 3);
    if (ifn_cf == NULL) {
	return;
    }
    ifstate = IFStateListGetIFState(&S_ifstate_list, ifn_cf, NULL);
    if (ifstate != NULL) {
	ipv6_router_prefix_counts_t	event = { 0 };
	interface_t *			if_p = ifstate->if_p;

	event.router_count
	    = inet6_router_and_prefix_count(if_link_index(if_p),
					    &event.prefix_count);
	my_log(LOG_NOTICE,
	       "%@: IPv6 router expired, router count %d prefix count %d",
	       ifn_cf, event.router_count, event.prefix_count);
	service_list_event(&ifstate->services_v6,
			   IFEventID_ipv6_router_expired_e, &event);
    }
    my_CFRelease(&ifn_cf);
    return;
}

static boolean_t
plat_discovery_is_complete(SCDynamicStoreRef session, CFStringRef cache_key)
{
    boolean_t		complete = FALSE;
    CFDictionaryRef	dict;

    dict = my_SCDynamicStoreCopyDictionary(session, cache_key);
    if (dict != NULL) {
	complete
	    = CFDictionaryContainsKey(dict,
				      kSCPropNetNAT64PLATDiscoveryCompletionTime);
	CFRelease(dict);
    }
    return (complete);
}

static void
process_plat_discovery_complete(SCDynamicStoreRef session,
				CFStringRef cache_key)
{
    CFStringRef		ifn_cf;
    IFStateRef   	ifstate;

    if (CFStringHasPrefix(cache_key, S_state_interface_prefix) == FALSE) {
	return;
    }

    /* figure out which interface this belongs to and deliver the event */

    /* State:/Network/Interface/<ifname>/NAT64 */
    ifn_cf = my_CFStringCopyComponent(cache_key, CFSTR("/"), 3);
    if (ifn_cf == NULL) {
	return;
    }
    ifstate = IFStateListGetIFState(&S_ifstate_list, ifn_cf, NULL);
    if (ifstate != NULL) {
	boolean_t	has_prefix;

	/* we're successful if the interface has prefixes */
	has_prefix = inet6_has_nat64_prefixlist(if_name(ifstate->if_p));
	IFStateFlagsSetOrClear(ifstate, kIFStateFlagsNAT64PrefixAvailable,
			       has_prefix);
	if (has_prefix || plat_discovery_is_complete(session, cache_key)) {
	    IFStateFlagsSet(ifstate, kIFStateFlagsPLATDiscoveryComplete);
	    my_log(LOG_INFO,
		   "%@: PLATDiscovery complete (%sNAT64)", ifn_cf,
		   has_prefix ? "" : "no ");
	    service_list_event(&ifstate->services_v6,
			       IFEventID_plat_discovery_complete_e,
			       &has_prefix);
	}
	else {
	    my_log(LOG_DEBUG,
		   "%@: PLATDiscovery incomplete", ifn_cf);
	}
    }
    my_CFRelease(&ifn_cf);
    return;
}

static WiFiInfoRef
S_copy_wifi_info(CFStringRef ifname)
{
    WiFiInfoRef	info_p;

    info_p = WiFiInfoCopy(ifname);
    if (info_p != NULL) {
	CFStringRef	networkID;

	networkID = WiFiInfoGetNetworkID(info_p);
	if (networkID == NULL) {
	    my_log(LOG_NOTICE,
		   "%@: SSID %@ BSSID %@ Security %s",
		   ifname,
		   WiFiInfoGetSSID(info_p),
		   WiFiInfoGetBSSIDString(info_p),
		   WiFiAuthTypeGetString(WiFiInfoGetAuthType(info_p)));
	}
	else {
	    my_log(LOG_NOTICE,
		   "%@: SSID %@ BSSID %@ NetworkID %@ Security %s",
		   ifname,
		   WiFiInfoGetSSID(info_p),
		   WiFiInfoGetBSSIDString(info_p),
		   networkID,
		   WiFiAuthTypeGetString(WiFiInfoGetAuthType(info_p)));
	}
    }
    else {
	my_log(LOG_NOTICE, "%@: no SSID", ifname);
    }
    return (info_p);
}

STATIC void
process_link_timer_expired(IFStateRef ifstate)
{
    my_log(LOG_NOTICE, "%s: link inactive timer fired",
	   if_name(ifstate->if_p));

    IFState_all_services_event(ifstate, IFEventID_link_timer_expired_e, NULL);
    if (dynarray_count(&ifstate->services_v6) != 0) {
	(void)inet6_linklocal_stop(if_name(ifstate->if_p));
    }
    return;
}

static void
link_timer_expired(void * arg0, void * arg1, void * arg2)
{
    process_link_timer_expired(arg0);
    return;
}

static void
ap_key_changed(SCDynamicStoreRef session, CFStringRef cache_key) 
{
    interface_t *	if_p;
    CFStringRef		ifn_cf;
    IFStateRef		ifstate;
    WiFiInfoRef		info_p = NULL;
    link_status_t	link;
    uint32_t		j;

    if (CFStringHasPrefix(cache_key, S_state_interface_prefix) == FALSE) {
	return;
    }

    /* State:/Network/Interface/<ifname>/Airport */
    ifn_cf = my_CFStringCopyComponent(cache_key, CFSTR("/"), 3);
    if (ifn_cf == NULL) {
	return;
    }
    ifstate = IFStateListGetIFState(&S_ifstate_list, ifn_cf, NULL);
    my_CFRelease(&ifn_cf);
    if (ifstate == NULL || IFStateFlagsAreSet(ifstate, kIFStateFlagsNetBoot)) {
	/* don't propagate media status events for netboot interface */
	goto done;
    }
    if_p = ifl_find_name(S_interfaces, if_name(ifstate->if_p));
    if (if_p == NULL) {
	/* interface doesn't exist */
	goto done;
    }
    link = if_link_status_update(if_p);
    if (if_is_wifi_infra(ifstate->if_p) == FALSE) {
	goto done;
    }
    if ((link.valid && !link.active)
	|| (ifstate->if_p->link_status.valid 
	    && !ifstate->if_p->link_status.active)) {
	/*
	 * Link is/was down, no need to handle it now.  This will be handled
	 * when the next link up event comes in.
	 */
	goto done;
    }

    /* check whether just the bssid has changed i.e. we roamed */
    info_p = S_copy_wifi_info(ifstate->ifname);
    if (!IFState_wifi_did_roam(ifstate, info_p)) {
	goto done;
    }

    /* we roamed */
    if (!if_is_awdl(if_p)) {
	my_log(LOG_NOTICE, "%s: Wi-Fi roam", if_name(if_p));
    }

    /* remember current information */
    IFState_set_wifi_info(ifstate, info_p);

    /* Notify v4 services */ 
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	ServiceRef      service_p = dynarray_element(&ifstate->services, j);

	config_method_bssid_changed(service_p);
    }

    /* Notify v6 services */
    service_list_event(&ifstate->services_v6, IFEventID_bssid_changed_e,
		       NULL);

 done:
    my_CFRelease(&info_p);
    return;
}

static void
link_key_changed(SCDynamicStoreRef session, CFStringRef cache_key)
{
    CFDictionaryRef		dict = NULL;
    interface_t *		if_p;
    CFStringRef			ifn_cf;
    char			ifn[IFNAMSIZ + 1];
    IFStateRef   		ifstate;
    boolean_t			interface_reattached = FALSE;
    int 			j;
    boolean_t			link_active;
    boolean_t			link_address_changed = FALSE;
    link_status_t		link_status;
    link_event_data		link_event;

    if (CFStringHasPrefix(cache_key, S_state_interface_prefix) == FALSE) {
	return;
    }
    /* State:/Network/Interface/<ifname>/Link */
    ifn_cf = my_CFStringCopyComponent(cache_key, CFSTR("/"), 3);
    if (ifn_cf == NULL) {
	return;
    }
    my_CFStringToCStringAndLength(ifn_cf, ifn, sizeof(ifn));
    my_CFRelease(&ifn_cf);
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
    link_status = if_link_status_update(if_p);
    if (link_status.valid) {
	/* make sure address information is up to date */
	link_address_changed = if_link_update(if_p);
    }
    if (ifstate == NULL || IFStateFlagsAreSet(ifstate, kIFStateFlagsNetBoot)) {
	/* don't propagate media status events for netboot interface */
	goto done;
    }
    ifstate->v6_ll_collision_count = 0;
    IFStateFlagsClear(ifstate,
		      kIFStateFlagsFailureSymptomReported
		      | kIFStateFlagsPLATDiscoveryComplete
		      | kIFStateFlagsNAT64PrefixAvailable);
    if_link_copy(ifstate->if_p, if_p);
    if (link_status.valid == FALSE) {
	my_log(LOG_NOTICE, "%s link is unknown", ifn);
    }
    else {
	my_log(LOG_NOTICE, "%s link %s%s%s%s%s",
	       ifn,
	       link_status.active ? "ACTIVE" : "INACTIVE",
	       link_address_changed ? " [link address changed]" : "",
	       link_status.wake_on_same_network
	       ? " [wake on same network]" : "",
	       if_is_expensive(if_p) ? " [expensive]" : "",
	       if_is_carplay(if_p) ? " [carplay]" : "");
    }
    if (link_address_changed == FALSE
	&& S_wake_generation != ifstate->wake_generation) {
	my_log(LOG_NOTICE,
	       "%s: link status changed at wake", ifn);
	S_ifstate_process_wake(ifstate);
	if (link_status.wake_on_same_network) {
	    goto done;
	}
    }

    /* process link changes */
    IFState_update_link_event_data(ifstate, &link_event, &link_status);
    link_active = (!link_status.valid || link_status.active);
    if (link_active) {
	IFStateFlagsClear(ifstate,  kIFStateFlagsLinkTimerSuppressed);
	timer_cancel(ifstate->timer);
    }
    else {
	if (S_awake == FALSE) {
	    my_log(LOG_INFO,
		   "%s: suppressing link inactive timer (going to sleep)",
		   ifn);
	    timer_cancel(ifstate->timer);
	    IFStateFlagsSet(ifstate,  kIFStateFlagsLinkTimerSuppressed);
	}
	else {
	    IFStateFlagsClear(ifstate,  kIFStateFlagsLinkTimerSuppressed);
	    my_log(LOG_INFO,
		   "%s: scheduling link inactive timer for %g secs",
		   ifn,
		   S_link_inactive_secs);
	    timer_callout_set(ifstate->timer,
			      S_link_inactive_secs, link_timer_expired,
			      ifstate, NULL, NULL);
	}
    }
    /*
     * Re-attach IPv4/IPv6 if there are services configured.
     * Interfaces can disappear across sleep/wake, and we only find
     * out about it after wake has already been processed.
     */
    if (if_ift_type(ifstate->if_p) != IFT_STF
	&& !ifstate->disable_until_needed.interface_disabled) {
	if (link_address_changed) {
	    /* first stop IPv6 link-local to force a state change */
	    my_log(LOG_NOTICE, "%s: link address changed", if_name(if_p));
	    (void)inet6_linklocal_stop(if_name(if_p));
	}
	if (dynarray_count(&ifstate->services) != 0) {
	    if (inet_attach_interface(if_name(if_p), link_active) == 0) {
		interface_reattached = TRUE;
	    }
	}
	if (dynarray_count(&ifstate->services_v6) != 0) {
	    if (IFState_attach_IPv6(ifstate, link_active)) {
		interface_reattached = TRUE;
	    }
	}
    }

    if (interface_reattached) {
	my_log(LOG_NOTICE,
	       "%s: interface reattached, forcing link timer expired",
	       if_name(if_p));
	IFState_all_services_event(ifstate,
				   IFEventID_link_timer_expired_e,
				   NULL);
    }

    /* notify IPv4 services */
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	ServiceRef	service_p = dynarray_element(&ifstate->services, j);

	config_method_media(service_p, &link_event);
    }
    /* notify IPv6 services */
    service_list_event(&ifstate->services_v6, IFEventID_link_status_changed_e,
		       &link_event);
 done:
    my_CFRelease(&dict);
    return;
}

static void
arp_collision(SCDynamicStoreRef session, CFStringRef cache_key)
{
    arp_collision_data_t	evdata;
    void *			hwaddr = NULL;
    int				hwlen;
    CFStringRef			ifn_cf = NULL;
    struct in_addr		ip_addr;
    IFStateRef   		ifstate;

    ifn_cf = IPv4ARPCollisionKeyParse(cache_key, &ip_addr, &hwaddr, &hwlen);
    if (ifn_cf == NULL || hwaddr == NULL) {
	goto done;
    }
    ifstate = IFStateListGetIFState(&S_ifstate_list, ifn_cf, NULL);
    if (ifstate == NULL || IFStateFlagsAreSet(ifstate, kIFStateFlagsNetBoot)) {
	/* don't propogate collision events for netboot interface */
	goto done;
    }
    if (S_is_our_hardware_address(NULL, if_link_arptype(ifstate->if_p), 
				  hwaddr, hwlen)) {
	goto done;
    }
    bzero(&evdata, sizeof(evdata));
    evdata.ip_addr = ip_addr;
    evdata.hwaddr = hwaddr;
    evdata.hwlen = hwlen;
    evdata.is_sleep_proxy
	= S_is_sleep_proxy_conflict(session, ifstate->ifname, hwaddr, hwlen,
				    &evdata.sleep_proxy_ip, &evdata.opt_record);
    service_list_event(&ifstate->services, IFEventID_arp_collision_e, &evdata);
    my_CFRelease(&evdata.opt_record);

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
    int 		i;

    if ((type & kSCPreferencesNotificationApply) == 0) {
	return;
    }

    /* merge in the new requested parameters */
    S_add_dhcp_parameters(prefs);
    SCPreferencesSynchronize(prefs);
    for (i = 0; i < dynarray_count(&S_ifstate_list); i++) {
	IFStateRef	ifstate = dynarray_element(&S_ifstate_list, i);
	int		j;
	link_event_data	link_event;

	bzero(&link_event, sizeof(link_event));
	link_event.link_status = if_get_link_status(ifstate->if_p);

	/* ask each service to renew immediately to pick up new options */
	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    ServiceRef	service_p = dynarray_element(&ifstate->services, j);

	    config_method_renew(service_p, &link_event);
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
	my_log(LOG_DEBUG, "Changes: %@ (%d)", changes, (int)count);
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
	    /* network configuration changed */
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
	else if (CFStringHasSuffix(cache_key, kSCEntNetNAT64)) {
	    process_plat_discovery_complete(session, cache_key);
	}
	else if (CFStringHasSuffix(cache_key,
				   kSCEntNetInterfaceActiveDuringSleepRequested)) {
	    ActiveDuringSleepRequestedKeyChanged(session, cache_key);
	}
	else if (CFStringHasSuffix(cache_key,
				   kSCEntNetIPv6RouterExpired)) {
	    ipv6_router_expired(session, cache_key);
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
	setDisableUntilNeededNeedsAttention();
    }

 done:
    my_CFRelease(&setup_global_ipv4_key);
    my_CFRelease(&state_changes);
    return;
}

#if TARGET_OS_OSX

static void
cleanup_user_notification(CFUserNotificationRef userNotification)
{
    /* clean-up the notification */
    for (int i = 0; i < dynarray_count(&S_ifstate_list); i++) {
	IFStateRef	ifstate = dynarray_element(&S_ifstate_list, i);
	int		j;

	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    ServiceRef service_p = dynarray_element(&ifstate->services, j);

	    if (service_p->user_notification == userNotification) {
		CFRunLoopRemoveSource(IPConfigurationAgentRunLoop(),
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
user_confirm_conflict(CFUserNotificationRef userNotification,
		      CFOptionFlags response_flags)
{
    dispatch_async(IPConfigurationAgentQueue(),
		   ^{ cleanup_user_notification(userNotification); });
}

static CFURLRef
copy_icon_url(CFStringRef icon)
{
    CFBundleRef		np_bundle;
    CFURLRef		np_url;
    CFURLRef		url = NULL;

#define kSystemConfigurationFramework	"/System/Library/Frameworks/SystemConfiguration.framework"
    np_url = CFURLCreateWithFileSystemPath(NULL,
					   CFSTR(kSystemConfigurationFramework),
					   kCFURLPOSIXPathStyle, FALSE);
    if (np_url != NULL) {
	np_bundle = CFBundleCreate(NULL, np_url);
	if (np_bundle != NULL) {
	    url = CFBundleCopyResourceURL(np_bundle, icon, 
					  CFSTR("tiff"), NULL);
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
	CFRunLoopRemoveSource(IPConfigurationAgentRunLoop(),
			      service_p->user_rls,
			      kCFRunLoopDefaultMode);
	my_CFRelease(&service_p->user_rls);
    }
    if (service_p->user_notification != NULL) {
	CFUserNotificationCancel(service_p->user_notification);
	my_CFRelease(&service_p->user_notification);
    }
    return;
}

#define kIconNetwork		CFSTR("Network")
#define kIconWiFi		CFSTR("WiFi")

static void
service_notify_user(ServiceRef service_p, CFArrayRef header,
		    CFStringRef message)
{
    CFMutableDictionaryRef	dict;
    SInt32			error;
    CFStringRef			icon_name;
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
    icon_name = if_is_wireless(service_ifstate(service_p)->if_p)
	? kIconWiFi : kIconNetwork;
    icon_url = copy_icon_url(icon_name);
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
	my_log(LOG_NOTICE, "CFUserNotificationCreate() failed, %d",
	       error);
	return;
    }
    rls = CFUserNotificationCreateRunLoopSource(NULL, notify, 
						user_confirm_conflict,
						0);
    if (rls == NULL) {
	my_log(LOG_NOTICE, "CFUserNotificationCreateRunLoopSource() failed");
	my_CFRelease(&notify);
    }
    else {
	CFRunLoopAddSource(IPConfigurationAgentRunLoop(), rls,
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

#endif /* TARGET_OS_OSX */

PRIVATE_EXTERN CFStringRef
ServiceCopyWakeID(ServiceRef service_p)
{
    IFStateRef		ifstate = service_ifstate(service_p);
    const char *	method_string;

#define WAKE_ID_FORMAT		WAKE_ID_PREFIX ".%s.%s"
    method_string = ipconfig_method_string(service_p->method);
    return (CFStringCreateWithFormat(NULL, NULL, CFSTR(WAKE_ID_FORMAT),
				     if_name(ifstate->if_p),
				     method_string));
}

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
	my_log(LOG_DEBUG, 
	       "%@ = %s", key, ret == TRUE ? "true" : "false");
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
	my_log(LOG_DEBUG, "%@ = %d", key, ret);
    }
    return (ret);
}

static CFTimeInterval
S_get_plist_time_interval(CFDictionaryRef plist, CFStringRef key,
			  CFTimeInterval def)
{
    CFNumberRef 	n;
    CFTimeInterval	ret = def;

    n = CFDictionaryGetValue(plist, key);
    if (isA_CFNumber(n) != NULL) {
	double	f;

	if (CFNumberGetValue(n, kCFNumberDoubleType, &f) == TRUE) {
	    ret = f;
	}
    }
    if (G_IPConfiguration_verbose) {
	my_log(LOG_DEBUG, "%@ = %g", key, ret);
    }
    return (ret);
}

static void *
S_get_number_array(CFArrayRef arr, int num_size, int * ret_count)
{
    void *	buf = NULL;
    CFIndex	count;
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
	my_log(LOG_DEBUG, "dictionary is %@", data);
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
    G_must_broadcast 
	= S_get_plist_boolean(plist, CFSTR("MustBroadcast"), FALSE);
    G_max_retries = S_get_plist_int(plist, CFSTR("RetryCount"), 
				    MAX_RETRIES);
    G_gather_secs = S_get_plist_int(plist, CFSTR("GatherTimeSeconds"), 
				    GATHER_SECS);
    S_link_inactive_secs 
	= S_get_plist_time_interval(plist,
				    CFSTR("LinkInactiveWaitTimeSeconds"),
				    S_link_inactive_secs);
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
	= S_get_plist_time_interval(plist, CFSTR("ARPRetryTimeSeconds"),
				    S_arp_retry);
    S_arp_detect_count
	= S_get_plist_int(plist, CFSTR("ARPDetectCount"), 
			  ARP_DETECT_COUNT);
    S_arp_detect_retry
	= S_get_plist_time_interval(plist, CFSTR("ARPDetectRetryTimeSeconds"),
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
    G_dhcp_generate_failure_symptom_at_retry_count
	= S_get_plist_int(plist,
			  CFSTR("DHCPGenerateFailureSymptomAtRetryCount"),
			  DHCP_GENERATE_FAILURE_SYMPTOM_AT_RETRY_COUNT);
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

    G_router_arp_wifi_lease_start_threshold_secs
	= S_get_plist_int(plist,
			  CFSTR("RouterARPWiFiLeaseStartThresholdSeconds"),
			  G_router_arp_wifi_lease_start_threshold_secs);

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
    S_defend_ip_address_interval_secs
	= S_get_plist_int(plist,
			  CFSTR("DefendIPAddressIntervalSeconds"),
			  DEFEND_IP_ADDRESS_INTERVAL_SECS);
    S_defend_ip_address_count
	= S_get_plist_int(plist,
			  CFSTR("DefendIPAddressCount"),
			  DEFEND_IP_ADDRESS_COUNT);
    G_dhcp_lease_write_t1_threshold_secs
	= S_get_plist_int(plist, 
			  CFSTR("DHCPLeaseWriteT1ThresholdSeconds"),
			  DHCP_LEASE_WRITE_T1_THRESHOLD_SECS);
    S_arp_conflict_retry
	= S_get_plist_int(plist,
			  CFSTR("ARPConflictRetryCount"),
			  ARP_CONFLICT_RETRY_COUNT);
    S_arp_conflict_delay
	= S_get_plist_time_interval(plist,
				    CFSTR("ARPConflictRetryDelaySeconds"),
				    S_arp_conflict_delay);
    G_manual_conflict_retry_interval_secs
	= S_get_plist_int(plist, 
			  CFSTR("ManualConflictRetryIntervalSeconds"),
			  MANUAL_CONFLICT_RETRY_INTERVAL_SECS);
    G_min_short_wake_interval_secs
	= S_get_plist_int(plist,
			  CFSTR("MinimumShortWakeIntervalSeconds"),
			  MIN_SHORT_WAKE_INTERVAL_SECS);
    G_min_wake_interval_secs
	= S_get_plist_int(plist,
			  CFSTR("MinimumWakeIntervalSeconds"),
			  MIN_WAKE_INTERVAL_SECS);
    G_wake_skew_secs
	= S_get_plist_int(plist,
			  CFSTR("WakeSkewSeconds"),
			  WAKE_SKEW_SECS);
#if TARGET_OS_OSX
    S_use_maintenance_wake
	= S_get_plist_boolean(plist,
			      CFSTR("UseMaintenanceWake"),
			      TRUE);
#endif /* TARGET_OS_OSX */
    S_configure_ipv6 = S_get_plist_boolean(plist,
					   CFSTR("ConfigureIPv6"),
					   TRUE);
    if (S_configure_ipv6) {
	uint16_t *	dhcpv6_options;
	int		dhcpv6_options_count = 0;

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
    }
    S_disable_unneeded_interfaces
	= S_get_plist_boolean(plist,
			      CFSTR("DisableUnneededInterfaces"),
			      TRUE);
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

#if TARGET_OS_OSX
STATIC void
check_hide_bssid(void)
{
    Boolean	hide_bssid;
    Boolean	was_set = FALSE;

    hide_bssid = IPConfigurationControlPrefsGetHideBSSID(FALSE, &was_set);
    if (hide_bssid != S_hide_bssid) {
	S_hide_bssid = hide_bssid;
	S_hide_bssid_was_set = was_set;
    }
    if (!S_hide_bssid_was_set) {
	hide_bssid = !_SC_isAppleInternal() && !G_IPConfiguration_verbose;
    }
    WiFiInfoSetHideBSSID(hide_bssid);
}
#endif /* TARGET_OS_OSX */

STATIC void
check_prefs(SCPreferencesRef prefs)
{
    Boolean				cellular_clat46_auto_enable;
    Boolean				ipv6_linklocal_modifier_expires;
    IPConfigurationInterfaceTypes	if_types;
    Boolean				verbose;

    verbose = IPConfigurationControlPrefsGetVerbose(FALSE);
    if (G_IPConfiguration_verbose != verbose) {
	G_IPConfiguration_verbose = verbose;
	if (verbose) {
	    my_log(LOG_NOTICE, "IPConfiguration: verbose mode enabled");
	}
	else {
	    my_log(LOG_NOTICE, "IPConfiguration: verbose mode disabled");
	}
	bootp_session_set_verbose(verbose);
	DHCPv6SocketSetVerbose(verbose);
	set_verbose_sysctls(verbose);
    }
    if_types = IPConfigurationControlPrefsGetAWDReportInterfaceTypes();
    if (if_types == kIPConfigurationInterfaceTypesUnspecified) {
	/* default to reporting for cellular */
	if_types = kIPConfigurationInterfaceTypesCellular;
    }
    if (if_types != G_awd_interface_types) {
	my_log(LOG_NOTICE, "IPConfiguration: AWD interface types %@",
	       IPConfigurationInterfaceTypesToString(if_types));
	G_awd_interface_types = if_types;
    }
    cellular_clat46_auto_enable
	= IPConfigurationControlPrefsGetCellularCLAT46AutoEnable(FALSE);
    if (S_cellular_clat46_autoenable != cellular_clat46_auto_enable) {
	my_log(LOG_NOTICE, "IPConfiguration: cellular CLAT46 %sauto-enabled",
	       cellular_clat46_auto_enable ? "" : "not ");
	S_cellular_clat46_autoenable = cellular_clat46_auto_enable;
    }
    ipv6_linklocal_modifier_expires
	= IPConfigurationControlPrefsGetIPv6LinkLocalModifierExpires(TRUE);
    if (S_ipv6_linklocal_modifier_expires
	!= ipv6_linklocal_modifier_expires) {
	my_log(LOG_NOTICE,
	       "IPConfiguration: IPv6 linklocal modifier %s",
	       ipv6_linklocal_modifier_expires ? "expires" : "does not expire");
	S_ipv6_linklocal_modifier_expires = ipv6_linklocal_modifier_expires;
    }

    /* DHCP DUID type */
    if (G_is_netboot) {
	G_dhcp_duid_type = kDHCPDUIDTypeLL;
    }
    else {
	DHCPDUIDType		duid_type;

	if (os_variant_is_darwinos(IPCONFIGURATION_SUBSYSTEM)) {
	    G_dhcp_duid_type = kDHCPDUIDTypeUUID;
	}
	else {
	    G_dhcp_duid_type = kDHCPDUIDTypeLLT;
	}
	duid_type = IPConfigurationControlPrefsGetDHCPDUIDType();
	switch (duid_type) {
	case kDHCPDUIDTypeNone:
	    break;
	case kDHCPDUIDTypeLLT:
	case kDHCPDUIDTypeLL:
	case kDHCPDUIDTypeUUID:
	    G_dhcp_duid_type = duid_type;
	    break;
	default:
	    my_log(LOG_NOTICE,
		   "%s: unsupported DHCP DUID type %d specified",
		   __func__, duid_type);
	    break;
	}
    }
#if TARGET_OS_OSX
    check_hide_bssid();
#endif /* TARGET_OS_OSX */

    IPConfigurationControlPrefsSynchronize();
    return;
}

STATIC os_state_data_t
IPConfigurationCopyOSStateData(os_state_hints_t hints)
{
#pragma unused(hints)
    CFDataRef			data;
    CFMutableDictionaryRef	info;
    os_state_data_t		state_data;
    size_t			state_data_size;
    CFIndex			state_len;

    info = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    for (int i = 0, count = dynarray_count(&S_ifstate_list); i < count; i++) {
        IFStateRef	ifstate = dynarray_element(&S_ifstate_list, i);
	CFDictionaryRef	summary = NULL;

	summary = IFStateCopySummary(ifstate);
	if (summary != NULL) {
	    CFDictionarySetValue(info, ifstate->ifname, summary);
	    CFRelease(summary);
	}
    }
    if (CFDictionaryGetCount(info) == 0) {
	CFRelease(info);
	return (NULL);
    }
    /* serialize the array into XML plist form */
    data = CFPropertyListCreateData(NULL,
				    info,
				    kCFPropertyListBinaryFormat_v1_0,
				    0,
				    NULL);
    CFRelease(info);
    state_len = CFDataGetLength(data);
    state_data_size = OS_STATE_DATA_SIZE_NEEDED(state_len);
    if (state_data_size > MAX_STATEDUMP_SIZE) {
	state_data = NULL;
	my_log(LOG_NOTICE, "%s: state data too large (%zd > %d)",
	       __func__, state_data_size, MAX_STATEDUMP_SIZE);
    }
    else {
	state_data = calloc(1, state_data_size);
	state_data->osd_type = OS_STATE_DATA_SERIALIZED_NSCF_OBJECT;
	state_data->osd_data_size = (uint32_t)state_len;
	strlcpy(state_data->osd_title,
		"Interface Configuration Summary",
		sizeof(state_data->osd_title));
	memcpy(state_data->osd_data, CFDataGetBytePtr(data), state_len);
    }
    CFRelease(data);
    return (state_data);
}

STATIC void
IPConfigurationAddStateHandler(void)
{
    os_state_block_t	dump_state;

    dump_state = ^os_state_data_t(os_state_hints_t hints)
	{
	 return (IPConfigurationCopyOSStateData(hints));
	};

    (void)os_state_add_handler(IPConfigurationAgentQueue(), dump_state);
}

STATIC void
init_log(void)
{
    os_log_t handle;

    handle = os_log_create(kIPConfigurationLogSubsystem,
			   kIPConfigurationLogCategoryServer);
    IPConfigLogSetHandle(handle);
    return;
}

#include <WatchdogClient/WatchdogService.h>

/* "weak_import" to ensure that the compiler doesn't optimize out NULL check */
void wd_endpoint_add_queue(dispatch_queue_t queue_to_monitor)
	__attribute__((weak_import));

void
load(CFBundleRef bundle, Boolean bundleVerbose)
{
    dispatch_queue_t	queue;

    init_log();
    S_bundle = (CFBundleRef)CFRetain(bundle);
    IPConfigurationAgentSetRunLoop(CFRunLoopGetCurrent());
    queue = IPConfigurationAgentQueue();
    if (wd_endpoint_add_queue != NULL) { /* "weak_import" */
	wd_endpoint_add_queue(queue);
    }
    return;
}

void
start(const char *bundleName, const char *bundleDir)
{
    arp_session_values_t	arp_values;
    SCPreferencesRef 		prefs = NULL;

    my_log(LOG_INFO, "IPConfiguration starting");

    /* register for prefs changes, check current state */
    check_prefs(IPConfigurationControlPrefsInit(IPConfigurationAgentQueue(),
						check_prefs));
    /* create paths */
    ipconfigd_create_paths();

    /* initialize CGA */
    CGAInit(S_ipv6_linklocal_modifier_expires);

    /* set globals */
    S_set_globals();
    prefs = SCPreferencesCreate(NULL, CFSTR("IPConfiguration.DHCPClient"),
				kDHCPClientPreferencesID);
    if (prefs == NULL) {
	my_log(LOG_NOTICE,
	       "IPConfiguration: SCPreferencesCreate failed: %s",
	       SCErrorString(SCError()));
	return;
    }
    if (!SCPreferencesSetCallback(prefs,
				 dhcp_preferences_changed,
				 NULL)
	|| !SCPreferencesSetDispatchQueue(prefs, IPConfigurationAgentQueue())) {
	my_log(LOG_NOTICE, "IPConfigurationSCPreferencesSetCallback failed: %s",
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
	my_log(LOG_NOTICE, "SCDynamicStoreCreate failed: %s",
	       SCErrorString(SCError()));
    }

    bootp_session_init(G_client_port);

    /* initialize the default values structure */
    bzero(&arp_values, sizeof(arp_values));
    arp_values.probe_count = &S_arp_probe_count;
    arp_values.probe_gratuitous_count = &S_arp_gratuitous_count;
    arp_values.probe_interval = &S_arp_retry;
    arp_values.detect_count = &S_arp_detect_count;
    arp_values.detect_interval = &S_arp_detect_retry;
    arp_values.conflict_retry_count = &S_arp_conflict_retry;
    arp_values.conflict_delay_interval = &S_arp_conflict_delay;
    arp_session_init(S_is_our_hardware_address, &arp_values);
    dynarray_init(&S_ifstate_list, IFState_free, NULL);

    CleanupWakeEvents();

    /* set the loopback interface address */
    set_loopback();
    return;
}

STATIC void
handle_prime(void)
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
    IPConfigurationAddStateHandler();
}

void
prime(void)
{
    dispatch_async(IPConfigurationAgentQueue(),
		   ^{ handle_prime(); });
}

STATIC void
handle_stop(CFRunLoopSourceRef stopRLS)
{
    IFStateList_all_services_event(&S_ifstate_list,
				   IFEventID_power_off_e, NULL);
    CFRunLoopSourceSignal(stopRLS);
}

void
stop(CFRunLoopSourceRef	stopRLS)
{
    dispatch_async(IPConfigurationAgentQueue(),
		   ^{ handle_stop(stopRLS); });
}
