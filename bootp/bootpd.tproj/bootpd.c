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
 * bootpd.c
 * - BOOTP/DHCP server main
 * - see RFC951, RFC2131, RFC2132 for details on the BOOTP protocol, 
 *   BOOTP extensions/DHCP options, and the DHCP protocol
 */

/*
 * Modification History
 * 01/22/86	Croft	created.
 *
 * 03/19/86	Lougheed  Converted to run under 4.3 BSD inetd.
 *
 * 09/06/88	King	Added NeXT interrim support.
 *
 * 02/23/98	Dieter Siegmund (dieter@apple.com)
 *		- complete overhaul
 *		- added specialized Mac NC support
 *		- removed the NeXT "Sexy Net Init" code that performed
 *		  a proprietary form of dynamic BOOTP, since this
 *		  functionality is replaced by DHCP
 *		- added ability to respond to requests originating from 
 *		  a specific set of interfaces
 *		- added rfc2132 option handling
 *
 * June 5, 1998 	Dieter Siegmund (dieter@apple.com)
 * - do lookups using netinfo calls directly to be able to read/write
 *   entries and get subnet-specific bindings
 *
 * Oct 19, 1998		Dieter Siegmund (dieter@apple.com)
 * - provide domain name servers for this server if not explicitly 
 *   configured otherwise
 * Mar 29, 1999		Dieter Siegmund (dieter@apple.com)
 * - added code to do ethernet lookups with or without leading zeroes
 * April 27, 2000	Dieter Siegmund (dieter@apple.com)
 * - added netinfo host caching to avoid denial of service
 *   attacks and handle any valid format for the ethernet address
 *   i.e. leading zeroes, capitalization, etc.
 * - eliminated practice of supplying a default bootfile
 * - eliminated ability to read host entries from a file
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/types.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/bootp.h>
#include <netinet/if_ether.h>
#include <net/if_arp.h>
#include <mach/boolean.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <sys/uio.h>
#include <resolv.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include <TargetConditionals.h>
#include "cfutil.h"

#include "arp.h"
#include "netinfo.h"
#include "interfaces.h"
#include "inetroute.h"
#include "subnets.h"
#include "dhcp_options.h"
#include "DNSNameList.h"
#include "rfc_options.h"
#include "macNC.h"
#include "bsdpd.h"
#include "NICache.h"
#include "host_identifier.h"
#include "dhcpd.h"
#include "bootpd.h"
#include "bsdp.h"
#include "bootp_transmit.h"
#include "util.h"
#include "cfutil.h"
#include "bootpd-plist.h"
#include "bootpdfile.h"
#include "bootplookup.h"

/* services */
#define CFGPROP_BOOTP_ENABLED		"bootp_enabled"
#define CFGPROP_DHCP_ENABLED		"dhcp_enabled"
#if NETBOOT_SERVER_SUPPORT
#define CFGPROP_OLD_NETBOOT_ENABLED	"old_netboot_enabled"
#define CFGPROP_NETBOOT_ENABLED		"netboot_enabled"
#define CFGPROP_USE_OPEN_DIRECTORY	"use_open_directory"
#endif /* NETBOOT_SERVER_SUPPORT */
#define CFGPROP_RELAY_ENABLED		"relay_enabled"
#define CFGPROP_DHCP_IGNORE_CLIENT_IDENTIFIER	"dhcp_ignore_client_identifier"
#define CFGPROP_DHCP_SUPPLY_BOOTFILE	"dhcp_supply_bootfile"
#define CFGPROP_DETECT_OTHER_DHCP_SERVER	"detect_other_dhcp_server"
#define CFGPROP_IGNORE_ALLOW_DENY	"ignore_allow_deny"
#define CFGPROP_IPV6_ONLY_PREFERRED	"ipv6_only_preferred"

#define CFGPROP_ALLOW			"allow"
#define CFGPROP_DENY			"deny"
#define CFGPROP_REPLY_THRESHOLD_SECONDS	"reply_threshold_seconds"
#define CFGPROP_RELAY_IP_LIST		"relay_ip_list"
#define CFGPROP_USE_SERVER_CONFIG_FOR_DHCP_OPTIONS "use_server_config_for_dhcp_options"
#define CFGPROP_IPV6_ONLY_WAIT		"ipv6_only_wait"
#define CFGPROP_VERBOSE			"verbose"
#define CFGPROP_DEBUG			"debug"

/*
 * /etc is read-only on non-macOS platforms.
 * Look for the configuration plist in an alternate location
 * that is writable on those platforms.
 */
#if TARGET_OS_OSX
#define	BOOTPD_PLIST_ROOT	"/etc"
#else /* TARGET_OS_OSX */
#define	BOOTPD_PLIST_ROOT	"/Library/Preferences/SystemConfiguration"
#endif /* TARGET_OS_OSX */

#define	BOOTPD_PLIST_PATH		BOOTPD_PLIST_ROOT "/bootpd.plist"

static const char *	S_bootpd_plist_path = BOOTPD_PLIST_PATH;

/* local defines */
#define	MAXIDLE			(5*60)	/* we hang around for five minutes */

#define SERVICE_BOOTP				0x00000001
#define SERVICE_DHCP				0x00000002
#define SERVICE_OLD_NETBOOT			0x00000004
#define SERVICE_NETBOOT				0x00000008
#define SERVICE_RELAY				0x00000010
#define SERVICE_ALL 				(SERVICE_BOOTP	       \
						 | SERVICE_DHCP	       \
						 | SERVICE_OLD_NETBOOT \
						 | SERVICE_NETBOOT     \
						 | SERVICE_RELAY)
#define SERVICE_IGNORE_ALLOW_DENY 		0x00000020
#define SERVICE_DETECT_OTHER_DHCP_SERVER 	0x00000040
#define SERVICE_IPV6_ONLY_PREFERRED		0x00000080
#define SERVICE_DHCP_DISABLED			0x80000000

/* global variables: */
char		boot_tftp_dir[128] = "/private/tftpboot";
int		bootp_socket = -1;
bool		debug;
bool		dhcp_ignore_client_identifier;
bool		dhcp_supply_bootfile;
int		quiet = 0;
uint32_t	reply_threshold_seconds = 0;
unsigned short	server_priority = BSDP_PRIORITY_BASE;
char *		testing_control = "";
char		server_name[MAXHOSTNAMELEN + 1];
SubnetListRef	subnets;
bool		verbose;

/*
 * transmit_buffer is cast to some struct types containing short fields;
 * force it to be aligned as much as an int
 */
static int	transmit_buffer_aligned[512];
char *		transmit_buffer = (char *)transmit_buffer_aligned;

#if USE_OPEN_DIRECTORY
#define USE_OPEN_DIRECTORY_DEFAULT	FALSE
bool		use_open_directory = USE_OPEN_DIRECTORY_DEFAULT;
#endif /* USE_OPEN_DIRECTORY */

/* local types */

/* local variables */
static boolean_t		S_bootfile_noexist_reply = TRUE;
static bool			S_debug;
static u_int32_t		S_do_services = 0;
static struct in_addr *		S_dns_servers = NULL;
static int			S_dns_servers_count = 0;
static char *			S_domain_name = NULL;
static uint8_t *		S_domain_search = NULL;
static int			S_domain_search_size = 0;
static ptrlist_t		S_if_list;
static interface_list_t *	S_interfaces;
static inetroute_list_t *	S_inetroutes = NULL;
static u_short			S_ipport_client = IPPORT_BOOTPC;
static u_short			S_ipport_server = IPPORT_BOOTPS;
#define IPV6_ONLY_WAIT_DEFAULT		0
static uint32_t			S_ipv6_only_wait;
static struct timeval		S_lastmsgtime;
/* ALIGN: S_rxpkt is aligned to at least sizeof(uint32_t) bytes */
static uint32_t 		S_rxpkt[2048/(sizeof(uint32_t))];/* receive packet buffer */
static boolean_t		S_sighup = TRUE; /* fake the 1st sighup */
static u_int32_t		S_which_services = 0;
static struct ether_addr *	S_allow = NULL;
static int			S_allow_count = 0;
static struct ether_addr *	S_deny = NULL;
static int			S_deny_count = 0;
static int			S_persist = 0;
static struct in_addr *		S_relay_ip_list = NULL;
static int			S_relay_ip_list_count = 0;
static int			S_max_hops = 4;
static boolean_t		S_use_server_config_for_dhcp_options = TRUE;
static boolean_t		S_verbose;

/* forward function declarations */
static int 		issock(int fd);
static void		bootp_request(request_t * request);
static void		S_receive_packet(void);
static void		S_publish_disabled_interfaces(boolean_t publish);
static void		S_add_ip_change_notifications();

#define PID_FILE "/var/run/bootpd.pid"
static void
writepid(void)
{
    FILE *fp;
    
    fp = fopen(PID_FILE, "w");
    if (fp != NULL) {
	fprintf(fp, "%d\n", getpid());
	(void) fclose(fp);
    }
}

/*
 * Function: background
 *
 * Purpose:
 *   Daemon-ize ourselves.
 */
static void
background()
{
    if (fork())
	exit(0);
    { 
	int s;
	for (s = 0; s < 10; s++)
	    (void) close(s);
    }
    (void) open("/", O_RDONLY);
    (void) dup2(0, 1);
    (void) dup2(0, 2);
    {
	int tt = open("/dev/tty", O_RDWR);
	if (tt > 0) {
	    ioctl(tt, TIOCNOTTY, 0);
	    close(tt);
	}
    }
}

static void
S_get_dns()
{
    int		domain_search_count = 0;
    int 	i;

    res_init(); /* figure out the default dns servers */

    S_domain_name = NULL;
    if (S_dns_servers) {
	free(S_dns_servers);
	S_dns_servers = NULL;
    }
    if (S_domain_search != NULL) {
	free(S_domain_search);
	S_domain_search = NULL;
    }
    S_domain_search_size = 0;
    S_dns_servers_count = 0;

    /* create the DNS server address list */
    if (_res.nscount != 0) {
	CFMutableStringRef	str = NULL;

	if (debug) {
	    str = CFStringCreateMutable(NULL, 0);
	}
	S_dns_servers = (struct in_addr *)
	    malloc(sizeof(*S_dns_servers) * _res.nscount);
	for (i = 0; i < _res.nscount; i++) {
	    in_addr_t	s_addr = _res.nsaddr_list[i].sin_addr.s_addr;

	    /* exclude 0.0.0.0, 255.255.255.255, and 127/8 */
	    if (s_addr == 0 
		|| s_addr == INADDR_BROADCAST
		|| (((ntohl(s_addr) & IN_CLASSA_NET) >> IN_CLASSA_NSHIFT) 
		    == IN_LOOPBACKNET)) {
		continue;
	    }
	    S_dns_servers[S_dns_servers_count].s_addr = s_addr;
	    if (str != NULL) {
		STRING_APPEND(str, " %s",
			      inet_ntoa(S_dns_servers[S_dns_servers_count]));
	    }
	    S_dns_servers_count++;
	}
	if (S_dns_servers_count == 0) {
	    free(S_dns_servers);
	    S_dns_servers = NULL;
	    my_CFRelease(&str);
	}
	if (str != NULL) {
	    my_log(LOG_DEBUG, "DNS servers: %@", str);
	    CFRelease(str);
	}
    }
    if (S_dns_servers_count != 0) {    
	CFMutableStringRef	str = NULL;

	if (debug) {
	    str = CFStringCreateMutable(NULL, 0);
	}
	if (_res.defdname[0] && strcmp(_res.defdname, "local") != 0) {
	    S_domain_name = _res.defdname;
	    if (debug) {
		my_log(LOG_DEBUG, "DNS domain: %s", S_domain_name);
	    }
	}
	/* create the DNS search list */
	for (i = 0; i < MAXDNSRCH; i++) {
	    if (_res.dnsrch[i] == NULL) {
		break;
	    }
	    domain_search_count++;
	    if (str != NULL) {
		STRING_APPEND(str, " %s", _res.dnsrch[i]);
	    }
	}
	if (domain_search_count != 0) {
	    S_domain_search 
		= DNSNameListBufferCreate((const char * *)_res.dnsrch,
					  domain_search_count,
					  NULL, &S_domain_search_size,
					  TRUE);
	}
	else {
	    my_CFRelease(&str);
	}
	if (str != NULL) {
	    my_log(LOG_DEBUG, "DNS search: %@", str);
	    CFRelease(str);
	}
    }
    return;
}

/*
 * Function: S_string_in_list
 *
 * Purpose:
 *   Given a List object, return boolean whether the C string is
 *   in the list.
 */
static boolean_t
S_string_in_list(ptrlist_t * list, const char * str)
{
    int i;

    for (i = 0; i < ptrlist_count(list); i++) {
	char * lstr = (char *)ptrlist_element(list, i);
	if (strcmp(str, lstr) == 0)
	    return (TRUE);
    }
    return (FALSE);
}

/*
 * Function: S_log_interfaces
 *
 * Purpose:
 *   Log which interfaces we will respond on.
 */
void
S_log_interfaces() 
{
    int i;
    int count = 0;
    
    for (i = 0; i < S_interfaces->count; i++) {
	interface_t * 	if_p = S_interfaces->list + i;
	
	if ((ptrlist_count(&S_if_list) == 0
	     || S_string_in_list(&S_if_list, if_name(if_p)))
	    && if_inet_valid(if_p) && !(if_flags(if_p) & IFF_LOOPBACK)) {
	    int 		i;
	    inet_addrinfo_t *	info;
	    char 		ip[32];

	    for (i = 0; i < if_inet_count(if_p); i++) {
		info = if_inet_addr_at(if_p, i);
		strlcpy(ip, inet_ntoa(info->addr), sizeof(ip));
		my_log(LOG_INFO, "interface %s: ip %s mask %s", 
		       if_name(if_p), ip, inet_ntoa(info->mask));
	    }
	    count++;
	}
    }
    if (count == 0) {
	my_log(LOG_INFO, "no available interfaces");
	if (S_persist == 0) {
	    exit(2);
	}
    }
    return;
}

/*
 * Function: S_get_interfaces
 * 
 * Purpose:
 *   Get the list of interfaces we will use.
 */
void
S_get_interfaces()
{
    interface_list_t *	new_list;
    
    new_list = ifl_init();
    if (new_list == NULL) {
	my_log(LOG_INFO, "interface list initialization failed");
	exit(1);
    }
    ifl_free(&S_interfaces);
    S_interfaces = new_list;
    return;
}

/*
 * Function: S_get_network_routes
 *
 * Purpose:
 *   Get the list of network routes.
 */
void
S_get_network_routes()
{
    inetroute_list_t * new_list;
    
    new_list = inetroute_list_init();
    if (new_list == NULL) {
	my_log(LOG_INFO, "can't get inetroutes list");
	exit(1);
    }
    inetroute_list_free(&S_inetroutes);
    S_inetroutes = new_list;
    if (debug) {
	CFMutableStringRef	str;

	str = CFStringCreateMutable(NULL, 0);
	inetroute_list_print_cfstr(str, S_inetroutes);
	my_log(~LOG_DEBUG, "Routes:\n%@", str);
	CFRelease(str);
    }
}

static void
S_service_enable(CFTypeRef prop, u_int32_t which)
{
    int 	i;
    CFStringRef	ifname_cf = NULL;
    CFIndex	count;

    if (prop == NULL) {
	return;
    }
    if (isA_CFBoolean(prop) != NULL) {
	if (CFEqual(prop, kCFBooleanTrue)) {
	    S_which_services |= which;
	}
	return;
    }
    if (isA_CFString(prop) != NULL) {
	count = 1;
	ifname_cf = prop;
    }
    else if (isA_CFArray(prop) != NULL) {
	count = CFArrayGetCount(prop);
	if (count == 0) {
	    S_which_services |= which;
	    return;
	}
    }
    else {
	/* invalid type */
	return;
    }
    for (i = 0; i < count; i++) {
	interface_t * 	if_p;
	char		ifname[IFNAMSIZ + 1];

	if (i != 0 || ifname_cf == NULL) {
	    ifname_cf = CFArrayGetValueAtIndex(prop, i);
	    if (isA_CFString(ifname_cf) == NULL) {
		continue;
	    }
	}
	if (CFStringGetCString(ifname_cf, ifname, sizeof(ifname),
			       kCFStringEncodingASCII)
	    == FALSE) {
	    continue;
	}
	if (*ifname == '\0') {
	    continue;
	}
	if_p = ifl_find_name(S_interfaces, ifname);
	if (if_p == NULL) {
	    continue;
	}
	if_p->user_defined |= which;
    }
    return;
}

#if NETBOOT_SERVER_SUPPORT

static void
S_service_disable(u_int32_t service)
{
    int i;

    S_which_services &= ~service;
    
    for (i = 0; i < S_interfaces->count; i++) {
	interface_t * 	if_p = S_interfaces->list + i;
	if_p->user_defined &= ~service;
    }
    return;
}

static boolean_t
S_service_is_enabled(u_int32_t service)
{
    int i;

    if ((S_which_services & service) != 0) {
	return (TRUE);
    }

    for (i = 0; i < S_interfaces->count; i++) {
	interface_t * 	if_p = S_interfaces->list + i;
	if (if_p->user_defined & service)
	    return (TRUE);
    }
    return (FALSE);
}

static void
S_disable_netboot()
{
    S_service_disable(SERVICE_NETBOOT | SERVICE_OLD_NETBOOT);
    return;
}
#endif /* NETBOOT_SERVER_SUPPORT */

typedef int (*qsort_compare_func_t)(const void *, const void *);

static struct ether_addr *
S_make_ether_list(CFArrayRef array, int * count_p)
{
    CFIndex		array_count = CFArrayGetCount(array);
    int			count = 0;
    int			i;
    struct ether_addr * list;

    list = (struct ether_addr *)malloc(sizeof(*list) * array_count);
    for (i = 0; i < array_count; i++) {
	struct ether_addr * 	eaddr;
	CFStringRef		str = CFArrayGetValueAtIndex(array, i);
	char			val[64];

	if (isA_CFString(str) == NULL) {
	    continue;
	}
	if (CFStringGetCString(str, val, sizeof(val), kCFStringEncodingASCII)
	    == FALSE) {
	    continue;
	}
	if (strlen(val) < 2) {
	    continue;
	}
	/* ignore ethernet hardware type, if present */
	if (strncmp(val, "1,", 2) == 0) {
	    eaddr = ether_aton(val + 2);
	}
	else {
	    eaddr = ether_aton((char *)val);
	}
	if (eaddr == NULL) {
	    continue;
	}
	list[count++] = *eaddr;
    }
    if (count == 0) {
	free(list);
	list = NULL;
    }
    else {
	qsort(list, count, sizeof(*list), (qsort_compare_func_t)ether_cmp);
    }
    *count_p = count;
    return (list);
}

static __inline__ boolean_t
ignore_allow_deny(interface_t * if_p)
{
    u_int32_t 	which = (S_which_services | if_p->user_defined);

    return ((which & SERVICE_IGNORE_ALLOW_DENY) != 0);
}

__private_extern__ boolean_t
detect_other_dhcp_server(interface_t * if_p)
{
    u_int32_t 	which = (S_which_services | if_p->user_defined);

    return ((which & SERVICE_DETECT_OTHER_DHCP_SERVER) != 0);
}

__private_extern__ boolean_t
ipv6_only_preferred(interface_t * if_p)
{
    u_int32_t 	which = (S_which_services | if_p->user_defined);

    return ((which & SERVICE_IPV6_ONLY_PREFERRED) != 0);
}

__private_extern__ void
disable_dhcp_on_interface(interface_t * if_p)
{
    if_p->user_defined &= ~SERVICE_DHCP;
    if_p->user_defined |= SERVICE_DHCP_DISABLED;
    S_publish_disabled_interfaces(TRUE);
    return;
}

static boolean_t
S_ok_to_respond(interface_t * if_p, int hwtype, void * hwaddr, int hwlen)
{
    struct ether_addr *	search;
    boolean_t		respond = TRUE;

    if (hwlen != ETHER_ADDR_LEN || ignore_allow_deny(if_p)) {
	return (TRUE);
    }
    if (S_deny != NULL) {
	search = bsearch(hwaddr, S_deny, S_deny_count, sizeof(*S_deny),
			 (qsort_compare_func_t)ether_cmp);
	if (search != NULL) {
	    my_log(LOG_DEBUG, "%s is in deny list, ignoring",
		   ether_ntoa(hwaddr));
	    respond = FALSE;
	}
    }
    if (respond == TRUE && S_allow != NULL) {
	search = bsearch(hwaddr, S_allow, S_allow_count, sizeof(*S_allow),
			 (qsort_compare_func_t)ether_cmp);
	if (search == NULL) {
	    my_log(LOG_DEBUG, "%s is not in the allow list, ignoring",
		   ether_ntoa(hwaddr));
	    respond = FALSE;
	}
    }
    return (respond);
}

static void
S_refresh_allow_deny(CFDictionaryRef plist)
{
    CFArrayRef		prop;

    if (S_allow != NULL) {
	free(S_allow);
	S_allow = NULL;
    }
    if (S_deny != NULL) {
	free(S_deny);
	S_deny = NULL;
    }
    S_allow_count = 0;
    S_deny_count = 0;

    if (plist == NULL) {
	return;
    }

    /* allow */
    prop = CFDictionaryGetValue(plist, CFSTR(CFGPROP_ALLOW));
    if (isA_CFArray(prop) != NULL && CFArrayGetCount(prop) > 0) {
	S_allow = S_make_ether_list(prop, &S_allow_count);
    }
    /* deny */
    prop = CFDictionaryGetValue(plist, CFSTR(CFGPROP_DENY));
    if (isA_CFArray(prop) != NULL && CFArrayGetCount(prop) > 0) {
	S_deny = S_make_ether_list(prop, &S_deny_count);
    }
    return;
}

static boolean_t
S_str_to_ip(const char * ip_str, struct in_addr * ret_ip)
{
    if (inet_aton(ip_str, ret_ip) == 0
	|| ret_ip->s_addr == 0 
	|| ret_ip->s_addr == INADDR_BROADCAST) {
	return (FALSE);
    }
    return (TRUE);
}

static void
S_relay_ip_list_clear(void)
{
    if (S_relay_ip_list != NULL) {
	free(S_relay_ip_list);
	S_relay_ip_list = NULL;
	S_relay_ip_list_count = 0;
    }
    return;
}

static void
S_relay_ip_list_add(struct in_addr relay_ip)
{
    if (S_relay_ip_list == NULL) {
	S_relay_ip_list 
	    = (struct in_addr *)malloc(sizeof(struct in_addr));
	S_relay_ip_list[0] = relay_ip;
	S_relay_ip_list_count = 1;
    }
    else {
	S_relay_ip_list_count++;
	S_relay_ip_list = (struct in_addr *)
	    realloc(S_relay_ip_list,
		    sizeof(struct in_addr) * S_relay_ip_list_count);
	if (S_relay_ip_list == NULL) {
	    my_log(LOG_NOTICE, "realloc failed, exiting");
	    exit(1);
	}
	S_relay_ip_list[S_relay_ip_list_count - 1] = relay_ip;
    }
    return;
}

static void
S_update_relay_ip_list(CFArrayRef list)
{
    CFIndex	count;
    int		i;

    count = CFArrayGetCount(list);
    S_relay_ip_list_clear();
    for (i = 0; i < count; i++) {
	struct in_addr	relay_ip;
	CFStringRef	str = CFArrayGetValueAtIndex(list, i);
		
	if (isA_CFString(str) == NULL) {
	    continue;
	}
	if (my_CFStringToIPAddress(str, &relay_ip) == FALSE) {
	    my_log(LOG_NOTICE, "Invalid relay server ip address");
	    continue;
	}
	if (relay_ip.s_addr == 0 || relay_ip.s_addr == INADDR_BROADCAST) {
	    my_log(LOG_NOTICE, 
		   "Invalid relay server ip address %s",
		   inet_ntoa(relay_ip));
	    continue;
	}
	if (ifl_find_ip(S_interfaces, relay_ip) != NULL) {
	    my_log(LOG_NOTICE, 
		   "Relay server ip address %s specifies this host",
		   inet_ntoa(relay_ip));
	    continue;
	}
	S_relay_ip_list_add(relay_ip);
    }
    return;
}

__private_extern__ void
set_number_from_plist(CFDictionaryRef plist, CFStringRef prop_name_cf,
		      const char * prop_name, uint32_t * val_p)
{
    CFTypeRef	prop;

    if (plist == NULL) {
	return;
    }
    prop = CFDictionaryGetValue(plist, prop_name_cf);
    if (prop != NULL
	&& my_CFTypeToNumber(prop, val_p) == FALSE) {
	my_log(LOG_NOTICE, "Invalid '%s' property", prop_name);
    }
    return;
}

#define SET_NUMBER_FROM_PLIST(plist, prop_name, val_p) \
    set_number_from_plist(plist, CFSTR(prop_name), prop_name, val_p)

static boolean_t
S_get_plist_boolean(CFDictionaryRef plist, CFStringRef prop_name_cf,
		    const char * prop_name, boolean_t def_value)
{
    boolean_t	ret;

    ret = def_value;
    if (plist != NULL) {
	CFBooleanRef	prop = CFDictionaryGetValue(plist, prop_name_cf);
	uint32_t	val;

	if (prop != NULL) {
	    if (my_CFTypeToNumber(prop, &val) == FALSE) {
		my_log(LOG_NOTICE, "Invalid '%s' property",
		       prop_name);
	    }
	    else {
		ret = (val != 0);
	    }
	}
    }
    return (ret);
}

#define GET_PLIST_BOOLEAN(plist, prop_name, def_value)			\
    S_get_plist_boolean(plist, CFSTR(prop_name), prop_name, def_value)

static uint32_t
S_get_plist_uint32(CFDictionaryRef plist, CFStringRef prop_name_cf,
		   const char * prop_name, boolean_t def_value)
{
    boolean_t	ret;

    ret = def_value;
    if (plist != NULL) {
	CFBooleanRef	prop = CFDictionaryGetValue(plist, prop_name_cf);
	uint32_t	val;

	if (prop != NULL) {
	    if (my_CFTypeToNumber(prop, &val) == FALSE) {
		my_log(LOG_NOTICE, "Invalid '%s' property",
		       prop_name);
	    }
	    else {
		ret = (val != 0);
	    }
	}
    }
    return (ret);
}

static void
S_update_services()
{
    uint32_t		num;
    CFDictionaryRef	plist = NULL;
    CFTypeRef		prop;

    plist = my_CFPropertyListCreateFromFile(S_bootpd_plist_path);
    if (plist != NULL) {
	if (isA_CFDictionary(plist) == NULL) {
	    CFRelease(plist);
	    plist = NULL;
	}
    }
    /* start with the set specified via command-line flags */
    S_which_services = S_do_services;
    verbose = S_verbose;
    debug = S_debug;

    /* IPv6 only wait time */
    S_ipv6_only_wait = S_get_plist_uint32(plist,
					  CFSTR(CFGPROP_IPV6_ONLY_WAIT),
					  CFGPROP_IPV6_ONLY_WAIT,
					  IPV6_ONLY_WAIT_DEFAULT);
    if (S_ipv6_only_wait != 0) {
	/* put it in network order */
	S_ipv6_only_wait = htonl(S_ipv6_only_wait);
    }
    if (plist != NULL) {
	/* verbose */
	if (GET_PLIST_BOOLEAN(plist, CFGPROP_VERBOSE, FALSE)) {
	    verbose = TRUE;
	}

	/* debug */
	if (GET_PLIST_BOOLEAN(plist, CFGPROP_DEBUG, FALSE)) {
	    debug = TRUE;
	}

	/* BOOTP */
	S_service_enable(CFDictionaryGetValue(plist,
					      CFSTR(CFGPROP_BOOTP_ENABLED)),
			 SERVICE_BOOTP);
	
	/* DHCP */
	S_service_enable(CFDictionaryGetValue(plist,
					      CFSTR(CFGPROP_DHCP_ENABLED)),
			 SERVICE_DHCP);
#if NETBOOT_SERVER_SUPPORT
	/* NetBoot (2.0) */
	S_service_enable(CFDictionaryGetValue(plist,
					      CFSTR(CFGPROP_NETBOOT_ENABLED)),
			 SERVICE_NETBOOT);

	/* NetBoot (old, pre 2.0) */
	S_service_enable(CFDictionaryGetValue(plist,
					      CFSTR(CFGPROP_OLD_NETBOOT_ENABLED)),
			 SERVICE_OLD_NETBOOT);
#endif /* NETBOOT_SERVER_SUPPORT */
	/* Relay */
	S_service_enable(CFDictionaryGetValue(plist,
					      CFSTR(CFGPROP_RELAY_ENABLED)),
			 SERVICE_RELAY);
	prop = CFDictionaryGetValue(plist, CFSTR(CFGPROP_RELAY_IP_LIST));
	if (isA_CFArray(prop) != NULL) {
	    S_update_relay_ip_list(prop);
	}

	/* pseudo services */

	/* Ignore Allow/Deny */
	S_service_enable(CFDictionaryGetValue(plist,
					      CFSTR(CFGPROP_IGNORE_ALLOW_DENY)),
			 SERVICE_IGNORE_ALLOW_DENY);
	/* Detect Other DHCP Server */
	S_service_enable(CFDictionaryGetValue(plist,
					      CFSTR(CFGPROP_DETECT_OTHER_DHCP_SERVER)),
			 SERVICE_DETECT_OTHER_DHCP_SERVER);
	/* IPv6-Only Preferred */
	S_service_enable(CFDictionaryGetValue(plist,
					      CFSTR(CFGPROP_IPV6_ONLY_PREFERRED)),
			 SERVICE_IPV6_ONLY_PREFERRED);
    }
    /* allow/deny list */
    S_refresh_allow_deny(plist);

    /* reply threshold */
    reply_threshold_seconds = 0;
    SET_NUMBER_FROM_PLIST(plist,
			  CFGPROP_REPLY_THRESHOLD_SECONDS,
			  &reply_threshold_seconds);


    /* ignore the DHCP client identifier */
    dhcp_ignore_client_identifier = false;
    num = 0;
    SET_NUMBER_FROM_PLIST(plist,
			  CFGPROP_DHCP_IGNORE_CLIENT_IDENTIFIER,
			  &num);
    if (num != 0) {
	dhcp_ignore_client_identifier = true;
    }
    /* DHCP should supply the bootfile if so configured in bootptab */
    dhcp_supply_bootfile = false;
    num = 0;
    SET_NUMBER_FROM_PLIST(plist,
			  CFGPROP_DHCP_SUPPLY_BOOTFILE,
			  &num);
    if (num != 0) {
	dhcp_supply_bootfile = true;
	my_log(LOG_INFO, "%s is enabled",
	       CFGPROP_DHCP_SUPPLY_BOOTFILE);
    }
#if USE_OPEN_DIRECTORY
    /* use open directory [for bootpent queries] */
    use_open_directory
	= GET_PLIST_BOOLEAN(plist, CFGPROP_USE_OPEN_DIRECTORY,
			    USE_OPEN_DIRECTORY_DEFAULT);
    my_log(LOG_INFO, "use_open_directory is %s",
	   use_open_directory ? "TRUE" : "FALSE");
#endif /* USE_OPEN_DIRECTORY */

    /* check whether to supply our own configuration for missing dhcp options */
    S_use_server_config_for_dhcp_options
	= GET_PLIST_BOOLEAN(plist, CFGPROP_USE_SERVER_CONFIG_FOR_DHCP_OPTIONS,
			    TRUE);

    /* get the new list of subnets */
    SubnetListFree(&subnets);
    if (plist != NULL) {
	prop = CFDictionaryGetValue(plist, BOOTPD_PLIST_SUBNETS);
	if (isA_CFArray(prop) != NULL) {
	    subnets = SubnetListCreateWithArray(prop);
	    if (subnets != NULL) {
		if (verbose) {
		    CFMutableStringRef	str;

		    str = CFStringCreateMutable(NULL, 0);
		    SubnetListPrintCFString(str, subnets);
		    my_log(~LOG_DEBUG, "%@", str);
		    CFRelease(str);
		}
	    }
	}
    }

    dhcp_init();
#if NETBOOT_SERVER_SUPPORT
    if (S_service_is_enabled(SERVICE_NETBOOT | SERVICE_OLD_NETBOOT)) {
	if (bsdp_init(plist) == FALSE) {
	    my_log(LOG_INFO, "bootpd: NetBoot service turned off");
	    S_disable_netboot();
	}
    }
#endif /* NETBOOT_SERVER_SUPPORT */
    if (plist != NULL) {
	CFRelease(plist);
    }
    return;
}

static __inline__ boolean_t
is_service_enabled(interface_t * if_p, u_int32_t service_flag,
		   u_int32_t service_disabled_flag)
{
    u_int32_t 	which = (S_which_services | if_p->user_defined);

    if (service_disabled_flag != 0
	&& (if_p->user_defined & service_disabled_flag) != 0) {
	return (FALSE);
    }
    return ((which & service_flag) != 0);
}

static __inline__ boolean_t
bootp_enabled(interface_t * if_p)
{
    return (is_service_enabled(if_p, SERVICE_BOOTP, 0));
}

static __inline__ boolean_t
dhcp_enabled(interface_t * if_p)
{
    return (is_service_enabled(if_p, SERVICE_DHCP, SERVICE_DHCP_DISABLED));
}

#if NETBOOT_SERVER_SUPPORT
static __inline__ boolean_t
netboot_enabled(interface_t * if_p)
{
    return (is_service_enabled(if_p, SERVICE_NETBOOT, 0));
}

static __inline__ boolean_t
old_netboot_enabled(interface_t * if_p)
{
    return (is_service_enabled(if_p, SERVICE_OLD_NETBOOT, 0));
}
#endif /* NETBOOT_SERVER_SUPPORT */

static __inline__ boolean_t
relay_enabled(interface_t * if_p)
{
    return (is_service_enabled(if_p, SERVICE_RELAY, 0));
}

void
usage()
{
    fprintf(stderr, "usage: bootpd <options>\n"
	    "<options> are:\n"
	    "[ -D ]	be a DHCP server\n"
	    "[ -b ] 	bootfile must exist or we don't respond\n"
	    "[ -d ]	debug mode, stay in foreground\n"
	    "[ -f <filename> ] load bootpd.plist from <filename>\n"
	    "[ -I ]	disable re-initialization on IP address changes\n"
	    "[ -i <interface> [ -i <interface> ... ] ]\n"
	    "[ -q ]	be quiet as possible\n"
	    "[ -r <server ip> [ -o <max hops> ] ] relay packets to server, "
	    "optionally set the hop count (default is 4 hops)\n"
	    "[ -S ]	enable BOOTP service\n"
	    "[ -v ] 	verbose mode, extra information\n"
	    );
    exit(1);
}

static void
install_sighup_handler(void)
{
    dispatch_block_t	signal_block;
    dispatch_source_t	signal_source;

    signal_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL,
					   SIGHUP,
					   0,
					   dispatch_get_main_queue());
    signal_block = ^{
	S_sighup = 1;
    };
    dispatch_source_set_event_handler(signal_source, signal_block);
    dispatch_resume(signal_source);
    signal(SIGHUP, SIG_IGN);
    return;
}

static void
idle_exit(void)
{
    struct timeval tv;

    gettimeofday(&tv, 0);
    if ((tv.tv_sec - S_lastmsgtime.tv_sec) >= MAXIDLE) {
	my_log(LOG_NOTICE, "idle, exiting");
	exit(0);
    }
}

static void
install_idle_check(void)
{
    dispatch_block_t	handler;
    dispatch_source_t	timer;

    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,
				   0,
				   0,
				   dispatch_get_main_queue());
    handler = ^{
	idle_exit();
    };
    dispatch_source_set_event_handler(timer, handler);
#define IDLE_INTERVAL_SECS	15
#define IDLE_INTERVAL_NSECS 	(IDLE_INTERVAL_SECS * NSEC_PER_SEC)
    dispatch_source_set_timer(timer,
			      dispatch_time(DISPATCH_TIME_NOW,
					    IDLE_INTERVAL_NSECS),
			      IDLE_INTERVAL_NSECS,
			      0);
    dispatch_resume(timer);
    return;
}

int
main(int argc, char * argv[])
{
    int			ch;
    bool		d_flag = FALSE;
    boolean_t		ip_change_notifications = TRUE;
    struct in_addr	relay_ip = { 0 };
    dispatch_source_t	sock_source;

    debug = FALSE;		/* no debugging */
    verbose = FALSE;		/* don't print extra information */

    ptrlist_init(&S_if_list);

    S_get_interfaces();

    while ((ch =  getopt(argc, argv, "aBbc:Ddf:hHi:I"
#if NETBOOT_SERVER_SUPPORT
        "mN"
#endif /* NETBOOT_SERVER_SUPPORT */
	"o:Pp:qr:St:v")) != EOF) {
	switch ((char)ch) {
	case 'a':
	    /* was: enable anonymous binding for BOOTP clients */
	    break;
	case 'B':
	    break;
	case 'S':
	    S_do_services |= SERVICE_BOOTP;
	    break;
	case 'b':
	    S_bootfile_noexist_reply = FALSE; 
	    /* reply only if bootfile exists */
	    break;
	case 'c':		    /* was: cache check interval - seconds */
	    break;
	case 'D':		/* answer DHCP requests as a DHCP server */
	    S_do_services |= SERVICE_DHCP;
	    break;
	case 'd':		/* stay in the foreground */
	    d_flag = TRUE;
	    break;
	case 'f':
	    S_bootpd_plist_path = optarg;
	    break;
	case 'h':
	case 'H':
	    usage();
	    exit(1);
	case 'I':
	    ip_change_notifications = FALSE;
	    break;
	case 'i':	/* user specified interface(s) to use */
	    if (S_string_in_list(&S_if_list, optarg) == FALSE) {
		ptrlist_add(&S_if_list, optarg);
	    }
	    else {
		my_log(LOG_INFO, "interface %s already specified",
		       optarg);
	    }
	    break;
#if NETBOOT_SERVER_SUPPORT
	case 'm':
	    S_do_services |= SERVICE_OLD_NETBOOT;
	    break;
	case 'N':
	    S_do_services |= SERVICE_NETBOOT;
	    break;
#endif /* NETBOOT_SERVER_SUPPORT */
	case 'o': {
	    int h;
	    h = atoi(optarg);
	    if (h > 16 || h < 1) {
		printf("max hops value %s must be in the range 1..16\n",
		       optarg);
		exit(1);
	    }
	    S_max_hops = h;
	    break;
	}
	case 'P':
	    S_persist = 1;
	    break;
	case 'p':
	    server_priority = strtoul(optarg, NULL, 0);
	    printf("Priority set to %d\n", server_priority);
	    break;
	case 'q':
	    quiet = 1;
	    break;
	case 'r':
	    S_do_services |= SERVICE_RELAY;
	    if (S_str_to_ip(optarg, &relay_ip) == FALSE) {
		printf("Invalid relay server ip address %s\n", optarg);
		exit(1);
	    }
	    if (ifl_find_ip(S_interfaces, relay_ip) != NULL) {
		printf("Relay server ip address %s specifies this host\n",
		       optarg);
		exit(1);
	    }
	    S_relay_ip_list_add(relay_ip);
	    break;
	case 't':
	    testing_control = optarg;
	    break;
	case 'v':	/* extra info to syslog */
	    S_verbose = TRUE;
	    break;
	default:
	    break;
	}
    }
    if (!issock(0)) { /* started by user */
	struct sockaddr_in Sin = { sizeof(Sin), AF_INET };
	int i;
	
	if (!d_flag) {
	    background();
	}
	
	if ((bootp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	    my_log(LOG_INFO, "socket call failed");
	    exit(1);
	}
	Sin.sin_port = htons(S_ipport_server);
	Sin.sin_addr.s_addr = htonl(INADDR_ANY);
	i = 0;
	while (bind(bootp_socket, (struct sockaddr *)&Sin, sizeof(Sin)) < 0) {
	    my_log(LOG_INFO, "bind call failed: %s", strerror(errno));
	    if (errno != EADDRINUSE)
		exit(1);
	    i++;
	    if (i == 10) {
		my_log(LOG_INFO, "exiting");
		exit(1);
	    }
	    sleep(10);
	}
    } 
    else { /* started by inetd */
	bootp_socket = 0;
	gettimeofday(&S_lastmsgtime, 0);
	if (S_persist == 0) {
	    install_idle_check();
	}
    }

    writepid();

    if (d_flag) {
	S_debug = TRUE;
    }

    my_log(LOG_DEBUG, "server starting");

    { 
	int opt = 1;

	if (setsockopt(bootp_socket, IPPROTO_IP, IP_RECVIF, (caddr_t)&opt,
		       sizeof(opt)) < 0) {
	    my_log(LOG_NOTICE, "setsockopt(IP_RECVIF) failed: %s", 
		   strerror(errno));
	    exit(1);
	}
	if (setsockopt(bootp_socket, SOL_SOCKET, SO_RECV_ANYIF, (caddr_t)&opt,
		       sizeof(opt)) < 0) {
	    my_log(LOG_NOTICE, "setsockopt(SO_RECV_ANYIF) failed");
	}
	if (setsockopt(bootp_socket, SOL_SOCKET, SO_BROADCAST, (caddr_t)&opt,
		       sizeof(opt)) < 0) {
	    my_log(LOG_NOTICE, "setsockopt(SO_BROADCAST) failed");
	    exit(1);
	}
	if (setsockopt(bootp_socket, IPPROTO_IP, IP_RECVDSTADDR, (caddr_t)&opt,
		       sizeof(opt)) < 0) {
	    my_log(LOG_NOTICE, "setsockopt(IPPROTO_IP, IP_RECVDSTADDR) failed");
	    exit(1);
	}
	if (setsockopt(bootp_socket, SOL_SOCKET, SO_REUSEADDR, (caddr_t)&opt,
		       sizeof(opt)) < 0) {
	    my_log(LOG_NOTICE, "setsockopt(SO_REUSEADDR) failed");
	    exit(1);
	}
#if defined(SO_TRAFFIC_CLASS)
	opt = SO_TC_CTL;
	/* set traffic class */
	if (setsockopt(bootp_socket, SOL_SOCKET, SO_TRAFFIC_CLASS, &opt,
		       sizeof(opt)) < 0) {
	    my_log(LOG_NOTICE, "setsockopt(SO_TRAFFIC_CLASS) failed, %s",
		   strerror(errno));
	}
#endif /* SO_TRAFFIC_CLASS */

#if defined(SO_DEFUNCTOK)
	opt = 0;
	/* ensure that our socket can't be defunct'd */
	if (setsockopt(bootp_socket, SOL_SOCKET, SO_DEFUNCTOK, &opt,
		       sizeof(opt)) < 0) {
		my_log(LOG_NOTICE, "setsockopt(SO_DEFUNCTOK) failed, %s",
		       strerror(errno));
	}
#endif /* SO_DEFUNCTOK */
	/* set non-blocking I/O */
	opt = 1;
	if (ioctl(bootp_socket, FIONBIO, &opt) < 0) {
		my_log(LOG_ERR, "ioctl FIONBIO failed, %s",
		       strerror(errno));
	}
    }

    sock_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ,
					 bootp_socket,
					 0UL,
					 dispatch_get_main_queue());
    dispatch_source_set_event_handler(sock_source,
				      ^{ S_receive_packet(); });
    dispatch_resume(sock_source);

    /* install our sighup handler */
    install_sighup_handler();

    if (ip_change_notifications) {
	S_add_ip_change_notifications();
    }
    dispatch_main();
    return (0);
}

/*
 * Function: subnetAddressAndMask
 *
 * Purpose:
 *   Given the gateway address field from the request and the 
 *   interface the packet was received on, determine the subnet
 *   address and mask.
 * Note:
 *   This currently does not support "super-netting", in which
 *   more than one proper subnet shares the same physical subnet.
 */
boolean_t
subnetAddressAndMask(struct in_addr giaddr, interface_t * if_p,
		     struct in_addr * addr, struct in_addr * mask)
{
    /* gateway specified, find a subnet description on the same subnet */
    if (giaddr.s_addr) {
	SubnetRef	subnet;

	/* find a subnet entry on the same subnet as the gateway */
	if (subnets == NULL) {
	    return (FALSE);
	}
	subnet = SubnetListGetSubnetForAddress(subnets, giaddr, FALSE);
	if (subnet == NULL) {
	    return (FALSE);
	}
	*addr = giaddr;
	*mask = SubnetGetMask(subnet);
    }
    else {
	*addr = if_inet_netaddr(if_p);
	*mask = if_inet_netmask(if_p);
    }
    return (TRUE);
}

/*
 * Function: issock
 *
 * Purpose:
 *   Determine if a descriptor belongs to a socket or not
 */
static int
issock(fd)
     int fd;
{
    struct stat st;
    
    if (fstat(fd, &st) < 0) {
	return (0);
    } 
    /*	
     * SunOS returns S_IFIFO for sockets, while 4.3 returns 0 and
     * does not even have an S_IFIFO mode.  Since there is confusion 
     * about what the mode is, we check for what it is not instead of 
     * what it is.
     */
    switch (st.st_mode & S_IFMT) {
      case S_IFCHR:
      case S_IFREG:
      case S_IFLNK:
      case S_IFDIR:
      case S_IFBLK:
	return (0);
      default:	
	return (1);
    }
}

/*
 * Function: bootp_add_bootfile
 *
 * Purpose:
 *   Verify that the specified bootfile exists, and add it to the given
 *   packet.  Handle <bootfile>.<hostname> to allow a specific host to
 *   get its own version of the bootfile.
 */
boolean_t
bootp_add_bootfile(const char * request_file, const char * hostname,
		   const char * bootfile,
		   char * reply_file, int reply_file_size)
{
    boolean_t 	dothost = FALSE;	/* file.host was found */
    char 	file[PATH_MAX];
    int		len;
    char 	path[PATH_MAX];

    if (request_file && request_file[0]) {
	strlcpy(file, request_file, sizeof(file));
    }
    else if (bootfile && bootfile[0]) {
	strlcpy(file, bootfile, sizeof(file));
    }
    else {
	my_log(LOG_DEBUG, "no replyfile");
	return (TRUE);
    }

    if (file[0] == '/')	{ /* if absolute pathname */
	strlcpy(path, file, sizeof(path));
    }
    else {
	strlcpy(path, boot_tftp_dir, sizeof(path));
	strlcat(path, "/", sizeof(path));
	strlcat(path, file, sizeof(path));
    }

    /* see if file exists with a ".host" suffix */
    if (hostname) {
	int 	n;

	n = (int)strlen(path);
	strlcat(path, ".", sizeof(path));
	strlcat(path, hostname, sizeof(path));
	if (access(path, R_OK) >= 0)
	    dothost = TRUE;
	else
	    path[n] = 0;	/* try it without the suffix */
    }
    
    if (dothost == FALSE) {
	if (access(path, R_OK) < 0) {
	    if (S_bootfile_noexist_reply == FALSE) {
		my_log(LOG_INFO, 
		       "boot file %s* missing - not replying", path);
		return (FALSE);
	    }
	    my_log(LOG_DEBUG, "boot file %s* missing", path);
	}
    }
    len = (int)strlen(path);
    if (len >= reply_file_size) {
	my_log(LOG_DEBUG, "boot file name too long %d >= %d",
	       len, reply_file_size);
	return (TRUE);
    }
    my_log(LOG_DEBUG, "replyfile %s", path);
    strlcpy(reply_file, path, reply_file_size);
    return (TRUE);
}

#define NIPROP_IP_ADDRESS	"ip_address"

/*
 * Function: ip_address_reachable
 *
 * Purpose:
 *   Determine whether the given ip address is directly reachable from
 *   the given interface or gateway.
 *
 *   Directly reachable means without using a router ie. share the same wire.
 */
boolean_t
ip_address_reachable(struct in_addr ip, struct in_addr giaddr, 
		    interface_t * if_p)
{
    int i;
    int if_index;

    if (giaddr.s_addr) { /* gateway'd */
	/* find a subnet entry on the same subnet as the gateway */
	if (subnets == NULL) {
	    return (FALSE);
	}
	return (SubnetListAreAddressesOnSameSupernet(subnets, ip, giaddr));
    }
    /* check whether client IP is on one of our interface's subnets */
    if (if_inet_match_subnet(if_p, ip) != INDEX_BAD) {
	if (verbose) {
	    my_log(LOG_DEBUG,
		   "%s: " IP_FORMAT " on subnet",
		   if_name(if_p), IP_LIST(&ip));
	}
	return (TRUE);
    }
    /* check whether client IP is on one of our interface's subnet routes */
    if_index = if_link_index(if_p);
    for (i = 0; i < S_inetroutes->count; i++) {
	inetroute_t * inr_p = S_inetroutes->list + i;

	if (inr_p->gateway.link.sdl_family == AF_LINK
	    && inr_p->gateway.link.sdl_index == if_index
	    && inr_p->mask.s_addr != 0
	    && in_subnet(inr_p->dest, inr_p->mask, ip)) {
	    if (verbose) {
		my_log(LOG_DEBUG,
		       "%s: " IP_FORMAT " on subnet route " IP_FORMAT,
		       if_name(if_p), IP_LIST(&ip), IP_LIST(&inr_p->dest));
	    }
	    return (TRUE);
	}
    }
    if (verbose) {
	my_log(LOG_DEBUG, "%s: ip %s not reachable",
	       if_name(if_p), inet_ntoa(ip));
    }
    return (FALSE);
}

boolean_t
subnet_match(void * arg, struct in_addr iaddr)
{
    subnet_match_args_t *	s = (subnet_match_args_t *)arg;

    if (iaddr.s_addr == 0) {
	/* make sure we never vend 0.0.0.0 */
	return (FALSE);
    }
    /* the binding may be invalid for this subnet, but it has one */
    s->has_binding = TRUE;
    if (iaddr.s_addr == s->ciaddr.s_addr
	|| ip_address_reachable(iaddr, s->giaddr, s->if_p)) {
	return (TRUE);
    }
    return (FALSE);
}

/*
 * Function: bootp_request
 *
 * Purpose:
 *   Process BOOTREQUEST packet.
 *
 */
static void
bootp_request(request_t * request)
{
    char *		bootfile = NULL;
    char *		hostname = NULL;
    struct in_addr	iaddr;
    struct bootp 	rp;
    struct bootp *	rq = (struct bootp *)request->pkt;
    u_int16_t		secs;

    if (request->pkt_length < sizeof(struct bootp))
	return;

    secs = (u_int16_t)ntohs(rq->bp_secs);
    if (secs < reply_threshold_seconds) {
	if (debug) {
	    my_log(LOG_DEBUG, "rq->bp_secs %d < threshold %d",
		   secs, reply_threshold_seconds);
	}
	return;
    }

    rp = *rq;	/* copy request into reply */
    rp.bp_op = BOOTREPLY;

    if (rq->bp_ciaddr.s_addr == 0) { /* client doesn't specify ip */
	subnet_match_args_t	match;

	bzero(&match, sizeof(match));
	match.if_p = request->if_p;
	match.giaddr = rq->bp_giaddr;
	if (bootp_getbyhw_file(rq->bp_htype, rq->bp_chaddr, rq->bp_hlen,
			       subnet_match, &match, &iaddr,
			       &hostname, &bootfile) == FALSE) {
#if USE_OPEN_DIRECTORY
	    if (use_open_directory == FALSE
	        || bootp_getbyhw_ds(rq->bp_htype, rq->bp_chaddr, rq->bp_hlen,
				    subnet_match, &match, &iaddr,
				    &hostname, &bootfile) == FALSE) {
		return;
	    }
#else /* USE_OPEN_DIRECTORY */
	    return;
#endif /* USE_OPEN_DIRECTORY */
	}
	rp.bp_yiaddr = iaddr;
    }
    else { /* client specified ip address */
	iaddr = rq->bp_ciaddr;
	if (bootp_getbyip_file(iaddr, &hostname, &bootfile) == FALSE) {
#if USE_OPEN_DIRECTORY
	    if (use_open_directory == FALSE
	        || bootp_getbyip_ds(iaddr, &hostname, &bootfile) == FALSE) {
		return;
	    }
#else /* USE_OPEN_DIRECTORY */
	    return;
#endif /* USE_OPEN_DIRECTORY */
	}
    }
    rq->bp_file[sizeof(rq->bp_file) - 1] = '\0';
    my_log(LOG_INFO,"BOOTP request [%s]: %s requested file '%s'",
	   if_name(request->if_p), 
	   hostname ? hostname : inet_ntoa(iaddr),
	   rq->bp_file);
    if (bootp_add_bootfile((const char *)rq->bp_file, hostname, bootfile,
			   (char *)rp.bp_file,
			   sizeof(rp.bp_file)) == FALSE)
	/* client specified a bootfile but it did not exist */
	goto no_reply;
    
    if (bcmp(rq->bp_vend, rfc_magic, sizeof(rfc_magic)) == 0) {
	/* insert the usual set of options/extensions if possible */
	dhcpoa_t	options;

	dhcpoa_init(&options, rp.bp_vend + sizeof(rfc_magic),
		    sizeof(rp.bp_vend) - sizeof(rfc_magic));

	add_subnet_options(hostname, iaddr, 
			   request->if_p, &options, NULL, 0);
	my_log(LOG_DEBUG, "added vendor extensions");
	if (dhcpoa_add(&options, dhcptag_end_e, 0, NULL)
	    != dhcpoa_success_e) {
	    my_log(LOG_INFO, "couldn't add end tag");
	}
	else
	    bcopy(rfc_magic, rp.bp_vend, sizeof(rfc_magic));
    } /* if RFC magic number */

    rp.bp_siaddr = if_inet_addr(request->if_p);
    strlcpy((char *)rp.bp_sname, server_name, sizeof(rp.bp_sname));
    if (sendreply(request->if_p, &rp, sizeof(rp), FALSE, NULL)) {
	my_log(LOG_INFO, "reply sent %s %s pktsize %d",
	       hostname, inet_ntoa(iaddr), (int)sizeof(rp));
    }

  no_reply:
    if (hostname != NULL)
	free(hostname);
    if (bootfile != NULL)
	free(bootfile);
    return;
}


/*
 * Function: sendreply
 *
 * Purpose:
 *   Send a reply packet to the client.
 */
boolean_t
sendreply(interface_t * if_p, struct bootp * bp, int n, 
	  boolean_t broadcast, struct in_addr * dest_p)
{
    struct in_addr 		dst;
    u_short			dest_port = S_ipport_client;
    void *			hwaddr = NULL;
    u_short			src_port = S_ipport_server;

    /*
     * If the client IP address is specified, use that
     * else if gateway IP address is specified, use that
     * else make a temporary arp cache entry for the client's NEW 
     * IP/hardware address and use that.
     */
    if (bp->bp_ciaddr.s_addr) {
	dst = bp->bp_ciaddr;
	my_log(LOG_DEBUG, "reply ciaddr %s", inet_ntoa(dst));
    }
    else if (bp->bp_giaddr.s_addr) {
	dst = bp->bp_giaddr;
	dest_port = S_ipport_server;
	src_port = S_ipport_client;
	my_log(LOG_DEBUG, "reply giaddr %s", inet_ntoa(dst));
	if (broadcast) /* tell the gateway to broadcast */
	    bp->bp_unused = htons(ntohs(bp->bp_unused | DHCP_FLAGS_BROADCAST));
    } 
    else { /* local net request */
	if (broadcast || (ntohs(bp->bp_unused) & DHCP_FLAGS_BROADCAST)) {
	    my_log(LOG_DEBUG, "replying using broadcast IP address");
	    dst.s_addr = htonl(INADDR_BROADCAST);
	}
	else {
	    if (dest_p)
		dst = *dest_p;
	    else
		dst = bp->bp_yiaddr;
	    hwaddr = bp->bp_chaddr;
	}
	my_log(LOG_DEBUG, "replying to %s", inet_ntoa(dst));
    }
    if (bootp_transmit(bootp_socket, transmit_buffer, if_name(if_p),
		       if_link_arptype(if_p),
		       hwaddr,
		       dst, if_inet_addr(if_p),
		       dest_port, src_port,
		       bp, n) < 0) {
	my_log(LOG_INFO, "transmit failed, %m");
	return (FALSE);
    }
    if (verbose) {
	CFMutableStringRef	str;

	str = CFStringCreateMutable(NULL, 0);
	dhcp_packet_print_cfstr(str, (struct dhcp *)bp, n);
	my_log(~LOG_DEBUG, "==== Server Reply ====\n%@", str);
	CFRelease(str);
    }
    return (TRUE);
}

/*
 * Function: add_subnet_options
 *
 * Purpose:
 *   Given a list of tags, retrieve them from the subnet entry and
 *   insert them into the message options.
 */
int
add_subnet_options(char * hostname, 
		   struct in_addr iaddr, interface_t * if_p,
		   dhcpoa_t * options, const uint8_t * tags, int n)
{
    struct in_addr * 	def_route = NULL;
    static const uint8_t default_tags[] = { 
	dhcptag_subnet_mask_e, 
	dhcptag_router_e, 
	dhcptag_domain_name_server_e,
	dhcptag_domain_name_e,
	dhcptag_host_name_e,
    };
#define N_DEFAULT_TAGS	(sizeof(default_tags) / sizeof(default_tags[0]))
    int			i;
    inet_addrinfo_t *	info = NULL;
    boolean_t		info_set = FALSE;
    int			number_before = dhcpoa_count(options);
    SubnetRef		subnet = NULL;

    if (subnets != NULL) {
	/* try to find exact match */
	subnet = SubnetListGetSubnetForAddress(subnets, iaddr, TRUE);
	if (subnet == NULL) {
	    /* settle for inexact match */
	    subnet = SubnetListGetSubnetForAddress(subnets, iaddr, FALSE);
	}
    }
    if (tags == NULL) {
	tags = default_tags;
	n = N_DEFAULT_TAGS;
    }
    for (i = 0; i < n; i++ ) {
	bool		handled = FALSE;

	switch (tags[i]) {
	  case dhcptag_end_e:
	  case dhcptag_pad_e:
	  case dhcptag_requested_ip_address_e:
	  case dhcptag_lease_time_e:
	  case dhcptag_option_overload_e:
	  case dhcptag_dhcp_message_type_e:
	  case dhcptag_server_identifier_e:
	  case dhcptag_parameter_request_list_e:
	  case dhcptag_message_e:
	  case dhcptag_max_dhcp_message_size_e:
	  case dhcptag_renewal_t1_time_value_e:
	  case dhcptag_rebinding_t2_time_value_e:
	  case dhcptag_client_identifier_e:
	      continue; /* ignore these */
	  default:
	      break;
	}
	if (tags[i] == dhcptag_host_name_e) {
	    if (hostname) {
		if (dhcpoa_add(options, dhcptag_host_name_e,
			       (int)strlen(hostname), hostname)
		    != dhcpoa_success_e) {
		    my_log(LOG_NOTICE, "couldn't add hostname: %s",
			   dhcpoa_err(options));
		}
	    }
	    handled = TRUE;
	}
	else if (subnet != NULL) {
	    const char *	opt;
	    int			opt_length;

	    opt = SubnetGetOptionPtrAndLength(subnet, tags[i], &opt_length);
	    if (opt != NULL) {
		handled = TRUE;
		if (dhcpoa_add(options, tags[i], opt_length, opt) 
		    != dhcpoa_success_e) {
		    my_log(LOG_NOTICE, "couldn't add option %d: %s",
			   tags[i], dhcpoa_err(options));
		}
	    }
	}
	if (handled == FALSE && S_use_server_config_for_dhcp_options) {
	    /* try to use defaults if no explicit configuration */
	    if (!info_set) {
		int			which_addr;

		info_set = TRUE;
		which_addr = if_inet_match_subnet(if_p, iaddr);
		if (which_addr != INDEX_BAD) {
		    info = if_inet_addr_at(if_p, which_addr);
		}
		def_route = inetroute_default(S_inetroutes);
	    }
	    switch (tags[i]) {
	      case dhcptag_subnet_mask_e: {
		if (info == NULL) {
		    continue;
		}
		if (dhcpoa_add(options, dhcptag_subnet_mask_e, 
			       sizeof(info->mask), &info->mask) 
		    != dhcpoa_success_e) {
		    my_log(LOG_NOTICE, "couldn't add subnet_mask: %s",
			   dhcpoa_err(options));
		    continue;
		}
		my_log(LOG_DEBUG, "subnet mask %s derived from %s",
		       inet_ntoa(info->mask), if_name(if_p));
		break;
	      }
	      case dhcptag_router_e:
		if (def_route == NULL
		    || info == NULL
		    || in_subnet(info->netaddr, info->mask,
				 *def_route) == FALSE
		    || in_subnet(info->netaddr, info->mask,
				 iaddr) == FALSE) {
		    /* don't respond if default route not on same subnet */
		    continue;
		}
		if (dhcpoa_add(options, dhcptag_router_e, sizeof(*def_route),
			       def_route) != dhcpoa_success_e) {
		    my_log(LOG_NOTICE, "couldn't add router: %s",
			   dhcpoa_err(options));
		    continue;
		}
		my_log(LOG_DEBUG, "default route added as router");
		break;
	      case dhcptag_domain_name_server_e:
		if (S_dns_servers_count == 0) {
		    continue;
		}
		if (dhcpoa_add(options, dhcptag_domain_name_server_e,
			       S_dns_servers_count * sizeof(*S_dns_servers),
			       S_dns_servers) != dhcpoa_success_e) {
		    my_log(LOG_NOTICE, "couldn't add dns servers: %s",
			   dhcpoa_err(options));
		    continue;
		}
		my_log(LOG_DEBUG, "default dns servers added");
		break;
	      case dhcptag_domain_name_e:
		if (S_domain_name) {
		    if (dhcpoa_add(options, dhcptag_domain_name_e,
				   (int)strlen(S_domain_name), S_domain_name)
			!= dhcpoa_success_e) {
			my_log(LOG_NOTICE, "couldn't add domain name: %s",
			       dhcpoa_err(options));
			continue;
		    }
		    my_log(LOG_DEBUG, "default domain name added");
		}
		break;
	      case dhcptag_domain_search_e:
		if (S_domain_search) {
		    if (dhcpoa_add(options, dhcptag_domain_search_e,
				   S_domain_search_size, S_domain_search)
			!= dhcpoa_success_e) {
			my_log(LOG_NOTICE, "couldn't add domain search: %s",
			       dhcpoa_err(options));
			continue;
		    }
		    my_log(LOG_DEBUG, "domain search added");
		}
		break;
	      default:
		break;
	    }
	}
	if (handled == FALSE) {
	    switch (tags[i]) {
	    case dhcptag_ipv6_only_preferred_e:
		if (!ipv6_only_preferred(if_p)) {
		    break;
		}
		if (dhcpoa_add(options, dhcptag_ipv6_only_preferred_e,
			       sizeof(S_ipv6_only_wait), &S_ipv6_only_wait)
		    != dhcpoa_success_e) {
		    my_log(LOG_NOTICE, "couldn't add ipv6 only preferred: %s",
			   dhcpoa_err(options));
		    continue;
		}
		else {
		    my_log(LOG_INFO, "IPv6-only preferred option added");
		}
		break;
	    default:
		break;
	    }
	}
    }
    return (dhcpoa_count(options) - number_before);
}

/**
 ** Server Main Loop
 **/
static char 		control[512];
static struct iovec  	iov;
static struct msghdr 	msg;

static void
S_init_msg()
{
    msg.msg_name = 0;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    msg.msg_flags = 0;
    iov.iov_base = (caddr_t)S_rxpkt;
    iov.iov_len = sizeof(S_rxpkt);
    return;
}

static void
S_relay_packet(struct bootp * bp, int n, interface_t * if_p)
{
    boolean_t	clear_giaddr = FALSE;
    int		i;
    boolean_t	printed = FALSE;
    u_int16_t	secs;

    if (n < sizeof(struct bootp))
	return;

    switch (bp->bp_op) {
    case BOOTREQUEST:
	if (bp->bp_hops >= S_max_hops)
	    return;
	secs = (u_int16_t)ntohs(bp->bp_secs);
	if (secs < reply_threshold_seconds) {
	    /* don't bother yet */
	    break;
	}
	if (bp->bp_giaddr.s_addr == 0) {
	    /* fill it in with our interface address */
	    bp->bp_giaddr = if_inet_addr(if_p);
	    clear_giaddr = TRUE;
	}
	bp->bp_hops++;
	for (i = 0; i < S_relay_ip_list_count; i++) {
	    struct in_addr relay = S_relay_ip_list[i];
	    if (relay.s_addr == if_inet_broadcast(if_p).s_addr) {
		continue; /* don't rebroadcast */
	    }
	    if (verbose && printed == FALSE) {
		CFMutableStringRef	str;

		str = CFStringCreateMutable(NULL, 0);
		printed = TRUE;
		dhcp_packet_print_cfstr(str, (struct dhcp *)bp, n);
		my_log(~LOG_DEBUG, "==== Relayed Request ====\n%@", str);
		CFRelease(str);
	    }

	    if (bootp_transmit(bootp_socket, transmit_buffer, if_name(if_p),
			       bp->bp_htype, NULL,
			       relay, if_inet_addr(if_p),
			       S_ipport_server, S_ipport_client,
			       bp, n) < 0) {
		my_log(LOG_NOTICE, "send to %s failed, %m", inet_ntoa(relay));
	    }
	    else {
		my_log(LOG_INFO,
		       "Relayed Request [%s] to %s", if_name(if_p),
		       inet_ntoa(relay));
	    }
	}
	if (clear_giaddr) {
	    bp->bp_giaddr.s_addr = 0;
	}
	bp->bp_hops--;
	break;
    case BOOTREPLY: {
	interface_t * 	if_p;
	struct in_addr	dst;

	if (bp->bp_giaddr.s_addr == 0) {
	    break;
	}
	if_p = ifl_find_ip(S_interfaces, bp->bp_giaddr);
	if (if_p == NULL) { /* we aren't the gateway - discard */
	    break;
	}
	
	if ((ntohs(bp->bp_unused) & DHCP_FLAGS_BROADCAST)) {
	    my_log(LOG_DEBUG, "replying using broadcast IP address");
	    dst.s_addr = htonl(INADDR_BROADCAST);
	}
	else {
	    dst = bp->bp_yiaddr;
	}
	if (verbose) {
	    CFMutableStringRef	str;

	    str = CFStringCreateMutable(NULL, 0);
	    dhcp_packet_print_cfstr(str, (struct dhcp *)bp, n);
	    my_log(~LOG_DEBUG, "==== Relayed Reply ====\n%@", str);
	    CFRelease(str);
	}
	if (bootp_transmit(bootp_socket, transmit_buffer, if_name(if_p),
			   bp->bp_htype, bp->bp_chaddr,
			   dst, if_inet_addr(if_p),
			   S_ipport_client, S_ipport_server,
			   bp, n) < 0) {
	    my_log(LOG_INFO, "send %s failed, %m", inet_ntoa(dst));
	}
	else {
	    my_log(LOG_INFO, 
		   "Relayed Response [%s] to %s", if_name(if_p),
		   inet_ntoa(dst));
	}
	break;
    }

    default:
	break;
    }
    return;
}

/*
 * Function: S_dispatch_request
 * Purpose:
 *   Dispatches a request according to whether it is BOOTP, DHCP, or NetBoot.
 */
static void
S_dispatch_request(struct bootp * bp, int n, interface_t * if_p,
		   struct in_addr * dstaddr_p)
{
#if NETBOOT_SERVER_SUPPORT
    boolean_t		bsdp_pkt = FALSE;
#endif /* NETBOOT_SERVER_SUPPORT */
    boolean_t		dhcp_pkt = FALSE;
    dhcp_msgtype_t	dhcp_msgtype = dhcp_msgtype_none_e;

    switch (bp->bp_op) {
      case BOOTREQUEST: {
	boolean_t 	handled = FALSE;
	dhcpol_t	options;
	request_t	request;

	request.if_p = if_p;
	request.pkt = (struct dhcp *)bp;
	request.pkt_length = n;
	request.options_p = NULL;
	request.dstaddr_p = dstaddr_p;
	request.time_in_p = &S_lastmsgtime;

	dhcpol_init(&options);

	/* get the packet options, check for dhcp */
	if (dhcpol_parse_packet(&options, (struct dhcp *)bp, n, NULL)) {
	    request.options_p = &options;
	    dhcp_pkt = is_dhcp_packet(&options, &dhcp_msgtype);
	}
	
	if (verbose) {
	    CFMutableStringRef	str;

	    str = CFStringCreateMutable(NULL, 0);
	    dhcp_packet_print_cfstr(str, (struct dhcp *)bp, n);
	    my_log(~LOG_DEBUG, "---- Client Request ----\n%@", str);
	    CFRelease(str);
	}

	if (bp->bp_sname[0] != '\0' 
	    && strcmp((char *)bp->bp_sname, server_name) != 0)
	    goto request_done;

	if (bp->bp_siaddr.s_addr != 0
	    && bp->bp_siaddr.s_addr != if_inet_addr(if_p).s_addr) {
	    goto request_done;
	}
	if (dhcp_pkt) { /* this is a DHCP packet */
#if NETBOOT_SERVER_SUPPORT
	    if (netboot_enabled(if_p) || old_netboot_enabled(if_p)) {
		char		arch[256];
		bsdp_version_t	client_version;
		boolean_t	is_old_netboot = FALSE;
		char		sysid[256];
		dhcpol_t	rq_vsopt; /* is_bsdp_packet() initializes */
		
		bsdp_pkt = is_bsdp_packet(request.options_p, arch, sysid,
					  &rq_vsopt, &client_version,
					  &is_old_netboot);
		if (bsdp_pkt) {
		    if (is_old_netboot == TRUE
			&& old_netboot_enabled(if_p) == FALSE) {
			/* ignore it */
		    }
		    else {
			bsdp_request(&request, dhcp_msgtype,
				     arch, sysid, &rq_vsopt, client_version,
				     is_old_netboot);
		    }
		}
		else {
		    bsdp_dhcp_request(&request, dhcp_msgtype);
		}
		dhcpol_free(&rq_vsopt);
	    }
#endif /* NETBOOT_SERVER_SUPPORT */
	    if (dhcp_enabled(if_p)
#if NETBOOT_SERVER_SUPPORT
	        || old_netboot_enabled(if_p)
#endif /* NETBOOT_SERVER_SUPPORT */
	        ) {
		handled = TRUE;
		dhcp_request(&request, dhcp_msgtype, dhcp_enabled(if_p));
	    }
	}
#if NETBOOT_SERVER_SUPPORT
	if (handled == FALSE && old_netboot_enabled(if_p)) {
	    handled = old_netboot_request(&request);
	}
#endif /* NETBOOT_SERVER_SUPPORT */
	if (handled == FALSE && bootp_enabled(if_p)) {
	    bootp_request(&request);
	}
      request_done:
	dhcpol_free(&options);
	break;
      }

      case BOOTREPLY:
	break;

      default:
	break;
    }

    if (S_relay_ip_list != NULL && relay_enabled(if_p)) {
	/* ALIGN: S_rxpkt is aligned to uint32, cast safe */
	S_relay_packet((struct bootp *)(void *)S_rxpkt, n, if_p);
    }

    if (verbose) {
	struct timeval now;
	struct timeval result;

	gettimeofday(&now, 0);
	timeval_subtract(now, S_lastmsgtime, &result);
	my_log(LOG_INFO, "service time %lu.%06d seconds",
	       result.tv_sec, result.tv_usec);
    }
    return;
}

static void *
S_parse_control(int level, int type, int * len)
{
    struct cmsghdr *	cmsg;

    *len = 0;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
	if (cmsg->cmsg_level == level 
	    && cmsg->cmsg_type == type) {
	    if (cmsg->cmsg_len < sizeof(*cmsg))
		return (NULL);
	    *len = cmsg->cmsg_len - sizeof(*cmsg);
	    return (CMSG_DATA(cmsg));
	}
    }
    return (NULL);
}

static interface_t *
S_which_interface()
{
    struct sockaddr_dl *dl_p;
    char		ifname[IFNAMSIZ + 1];
    interface_t *	if_p = NULL;
    int 		len = 0;

    dl_p = (struct sockaddr_dl *)S_parse_control(IPPROTO_IP, IP_RECVIF, &len);
    if (dl_p == NULL || len == 0 || dl_p->sdl_nlen >= sizeof(ifname)) {
	return (NULL);
    }
    
    bcopy(dl_p->sdl_data, ifname, dl_p->sdl_nlen);
    ifname[dl_p->sdl_nlen] = '\0';
    if_p = ifl_find_name(S_interfaces, ifname);
    if (if_p == NULL) {
	if (verbose)
	    my_log(LOG_DEBUG, "unknown interface %s", ifname);
	return (NULL);
    }
    if (if_inet_valid(if_p) == FALSE) {
	if (verbose) {
	    my_log(LOG_DEBUG, "ignoring request on %s (no IP address)", ifname);
	}
	return (NULL);
    }
    if (ptrlist_count(&S_if_list) > 0
	&& S_string_in_list(&S_if_list, ifname) == FALSE) {
	if (verbose) {
	    my_log(LOG_DEBUG, "ignoring request on %s", ifname);
	}
	return (NULL);
    }
    if (!is_service_enabled(if_p, SERVICE_ALL, SERVICE_DHCP_DISABLED)) {
	/* no services enabled */
	if (verbose) {
	    my_log(LOG_DEBUG, "ignoring request on %s (no services enabled)",
		   ifname);
	}
	if_p = NULL;
    }
    return (if_p);
}

static struct in_addr *
S_which_dstaddr()
{
    void *	data;
    int		len = 0;
    
    data = S_parse_control(IPPROTO_IP, IP_RECVDSTADDR, &len);
    if (data && len == sizeof(struct in_addr))
	return ((struct in_addr *)data);
    return (NULL);
}

/*
 * Function: S_receive_packet
 * Purpose:
 *   Receive event handler for BOOTP/DHCP server port.
 */
static void
S_receive_packet()
{
    struct in_addr * 	dstaddr_p = NULL;
    struct sockaddr_in 	from = { sizeof(from), AF_INET };
    interface_t *	if_p = NULL;
    ssize_t		n;
    /* ALIGN: S_rxpkt is aligned to uint32, hence cast safe */
    struct dhcp *	request = (struct dhcp *)(void *)S_rxpkt;

    S_init_msg();
    msg.msg_name = (caddr_t)&from;
    msg.msg_namelen = sizeof(from);
    n = recvmsg(bootp_socket, &msg, 0);
    if (n < 0) {
	my_log(LOG_DEBUG, "recvmsg failed, %m");
	goto no_reply;
    }
    if (S_sighup) {
	bootp_readtab(NULL);

	if (gethostname(server_name, sizeof(server_name) - 1)) {
	    server_name[0] = '\0';
	    my_log(LOG_INFO, "gethostname() failed, %m");
	}
	else {
	    my_log(LOG_INFO, "server name %s", server_name);
	}

	S_get_interfaces();
	S_log_interfaces();
	S_get_network_routes();
	S_publish_disabled_interfaces(FALSE);
	S_update_services();
	S_get_dns();
	S_sighup = FALSE;
    }

    if (n < sizeof(struct dhcp)) {
	goto no_reply;
    }
    if (request->dp_hlen > sizeof(request->dp_chaddr)) {
	goto no_reply;
    }
    dstaddr_p = S_which_dstaddr();
    if (debug) {
	if (dstaddr_p == NULL) {
	    my_log(LOG_DEBUG, "no destination address");
	}
	else {
	    my_log(LOG_DEBUG, "destination address %s",
		   inet_ntoa(*dstaddr_p));
	}
    }

    if_p = S_which_interface();
    if (if_p == NULL) {
	goto no_reply;
    }
    if (S_ok_to_respond(if_p, request->dp_htype, request->dp_chaddr,
			request->dp_hlen) == FALSE) {
	goto no_reply;
    }

    gettimeofday(&S_lastmsgtime, 0);
    /* ALIGN: S_rxpkt is aligned, cast ok. */
    S_dispatch_request((struct bootp *)(void *)S_rxpkt, (int)n,
		       if_p, dstaddr_p);
 no_reply:
    return;
}

#if NO_SYSTEMCONFIGURATION
static void
S_publish_disabled_interfaces(boolean_t publish)
{
}

static void
S_add_ip_change_notifications()
{
}
#else /* NO_SYSTEMCONFIGURATION */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>
#include <notify.h>

static SCDynamicStoreRef	store;

static void
S_ipv4_address_changed(SCDynamicStoreRef session, CFArrayRef changes,
		       void * info)
{
    S_sighup = TRUE;
}

static void
S_add_ip_change_notifications()
{
    CFStringRef			key;
    CFDictionaryRef		options;
    CFMutableArrayRef		patterns;

    options = CFDictionaryCreate(NULL,
				 (const void * *)&kSCDynamicStoreUseSessionKeys,
				 (const void * *)&kCFBooleanTrue,
				 1,
				 &kCFTypeDictionaryKeyCallBacks,
				 &kCFTypeDictionaryValueCallBacks);
    store = SCDynamicStoreCreateWithOptions(NULL,
					    CFSTR("com.apple.network.bootpd"),
					    options,
					    S_ipv4_address_changed,
					    NULL);
    CFRelease(options);
    if (store == NULL) {
	my_log(LOG_ERR, "SCDynamicStoreCreate failed");
	exit(2);
	return;
    }
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							kSCCompAnyRegex,
							kSCEntNetIPv4);
    patterns = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(patterns, key);
    CFRelease(key);
    SCDynamicStoreSetNotificationKeys(store, NULL, patterns);
    CFRelease(patterns);
    SCDynamicStoreSetDispatchQueue(store, dispatch_get_main_queue());
    return;
}

static CFArrayRef
S_copy_disabled_interfaces(void)
{
    int 		i;
    CFMutableArrayRef	list = NULL;

    for (i = 0; i < S_interfaces->count; i++) {
	interface_t * 	if_p = S_interfaces->list + i;

	if ((if_p->user_defined & SERVICE_DHCP_DISABLED) != 0) {
	    CFStringRef	if_name_cf;

	    if_name_cf = CFStringCreateWithCString(NULL, if_name(if_p),
						   kCFStringEncodingUTF8);
	    if (if_name_cf != NULL) {
		if (list == NULL) {
		    list = CFArrayCreateMutable(NULL, 0,
						&kCFTypeArrayCallBacks);
		}
		CFArrayAppendValue(list, if_name_cf);
		CFRelease(if_name_cf);
	    }
	}
    }
    return (list);
}

#define kStoreKey		CFSTR(DHCPD_DYNAMIC_STORE_KEY)

static void
S_publish_disabled_interfaces(boolean_t publish)
{
    static boolean_t 	last_publish = FALSE;

    if (publish == FALSE && last_publish == FALSE) {
	/* we don't have anything to publish, and didn't publish last time */
	return;
    }
    SCDynamicStoreRemoveValue(store, kStoreKey);
    if (publish) {
	CFArrayRef	list;

	list = S_copy_disabled_interfaces();
	if (list != NULL) {
	    CFDictionaryRef	dict;
	    CFStringRef		key;

	    key = CFSTR(DHCPD_DISABLED_INTERFACES);
	    dict = CFDictionaryCreate(NULL,
				      (const void * *)&key,
				      (const void * *)&list,
				      1,
				      &kCFTypeDictionaryKeyCallBacks,
				      &kCFTypeDictionaryValueCallBacks);
	    CFRelease(list);
	    SCDynamicStoreAddTemporaryValue(store, kStoreKey, dict);
	    CFRelease(dict);
	}
    }
    notify_post(DHCPD_DISABLED_INTERFACES_NOTIFICATION_KEY);
    last_publish = publish;
    return;
}
#endif /* !NO_SYSTEMCONFIGURATION */
