/*
 * Copyright (c) 1999 - 2008 Apple Inc. All rights reserved.
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
#include <SystemConfiguration/SCValidation.h>

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

#define CFGPROP_DHCP_IGNORE_CLIENT_IDENTIFIER	"dhcp_ignore_client_identifier"
#define CFGPROP_DETECT_OTHER_DHCP_SERVER	"detect_other_dhcp_server"
#define CFGPROP_BOOTP_ENABLED		"bootp_enabled"
#define CFGPROP_DHCP_ENABLED		"dhcp_enabled"
#if !TARGET_OS_EMBEDDED
#define CFGPROP_OLD_NETBOOT_ENABLED	"old_netboot_enabled"
#define CFGPROP_NETBOOT_ENABLED		"netboot_enabled"
#define CFGPROP_USE_OPEN_DIRECTORY	"use_open_directory"
#endif /* !TARGET_OS_EMBEDDED */
#define CFGPROP_RELAY_ENABLED		"relay_enabled"
#define CFGPROP_ALLOW			"allow"
#define CFGPROP_DENY			"deny"
#define CFGPROP_REPLY_THRESHOLD_SECONDS	"reply_threshold_seconds"
#define CFGPROP_RELAY_IP_LIST		"relay_ip_list"
#define CFGPROP_USE_SERVER_CONFIG_FOR_DHCP_OPTIONS "use_server_config_for_dhcp_options"

/*
 * On some platforms the root filesystem is mounted read-only;
 * make sure that the plist points to a user-writeable location.
 */
#if TARGET_OS_EMBEDDED
#define	BOOTPD_PLIST_ROOT	"/Library/Preferences/SystemConfiguration"
#else
#define	BOOTPD_PLIST_ROOT	"/etc"
#endif /* TARGET_OS_EMBEDDED */
#define	BOOTPD_PLIST_PATH		BOOTPD_PLIST_ROOT "/bootpd.plist"

/* local defines */
#define	MAXIDLE			(5*60)	/* we hang around for five minutes */
#define SERVICE_BOOTP		0x00000001
#define SERVICE_DHCP		0x00000002
#define SERVICE_OLD_NETBOOT	0x00000004
#define SERVICE_NETBOOT		0x00000008
#define SERVICE_RELAY		0x00000010

/* global variables: */
char		boot_tftp_dir[128] = "/private/tftpboot";
int		bootp_socket = -1;
int		debug = 0;
bool		detect_other_dhcp_server = FALSE;
bool		dhcp_ignore_client_identifier = FALSE;
int		quiet = 0;
uint32_t	reply_threshold_seconds = 0;
unsigned short	server_priority = BSDP_PRIORITY_BASE;
char *		testing_control = "";
char		server_name[MAXHOSTNAMELEN + 1];
SubnetListRef	subnets;
/*
 * transmit_buffer is cast to some struct types containing short fields;
 * force it to be aligned as much as an int
 */
static int	transmit_buffer_aligned[512];
char *		transmit_buffer = (char *)transmit_buffer_aligned;

#if ! TARGET_OS_EMBEDDED
bool		use_open_directory = TRUE;
#endif /* ! TARGET_OS_EMBEDDED */
int		verbose = 0;

/* local types */

/* local variables */
static boolean_t		S_bootfile_noexist_reply = TRUE;
static boolean_t		S_do_bootp;
#if !TARGET_OS_EMBEDDED
static boolean_t		S_do_netboot;
static boolean_t		S_do_old_netboot;
#endif /* !TARGET_OS_EMBEDDED */
static boolean_t		S_do_dhcp;
static boolean_t		S_do_relay;
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
static struct timeval		S_lastmsgtime;
static u_char 			S_rxpkt[2048];/* receive packet buffer */
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


void
my_log(int priority, const char *message, ...)
{
    va_list 		ap;

    if (priority == LOG_DEBUG) {
	if (verbose == FALSE)
	    return;
	priority = LOG_NOTICE;
    }
    else if (priority == LOG_INFO) {
	priority = LOG_NOTICE;
    }
    if (quiet && (priority > LOG_ERR)) { 
	return;
    }
    va_start(ap, message);
    vsyslog(priority, message, ap);
    va_end(ap);
    return;
}

/* forward function declarations */
static int 		issock(int fd);
static void		on_alarm(int sigraised);
static void		on_sighup(int sigraised);
static void		bootp_request(request_t * request);
static void		S_server_loop();

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
	S_dns_servers = (struct in_addr *)malloc(sizeof(*S_dns_servers) * _res.nscount);
	for (i = 0; i < _res.nscount; i++) {
	    in_addr_t	s_addr = _res.nsaddr_list[i].sin_addr.s_addr;

	    /* exclude 0.0.0.0, 255.255.255.255, and 127/8 */
	    if (s_addr == 0 
		|| s_addr == INADDR_BROADCAST
		|| (((ntohl(s_addr) & IN_CLASSA_NET) >> IN_CLASSA_NSHIFT) 
		    == IN_LOOPBACKNET)) {
		continue;
	    }
	    S_dns_servers[S_dns_servers_count++].s_addr = s_addr;
	    if (debug) {
		if (S_dns_servers_count == 1) {
		    printf("DNS servers:");
		}
		printf(" %s",
		       inet_ntoa(S_dns_servers[S_dns_servers_count - 1]));
	    }
	}
	if (S_dns_servers_count == 0) {
	    free(S_dns_servers);
	    S_dns_servers = NULL;
	}
	else if (debug) {
	    printf("\n");
	}
    }
    if (S_dns_servers_count != 0) {    
	if (_res.defdname[0] && strcmp(_res.defdname, "local") != 0) {
	    S_domain_name = _res.defdname;
	    if (debug)
		printf("DNS domain: %s\n", S_domain_name);
	}
	/* create the DNS search list */
	for (i = 0; i < MAXDNSRCH; i++) {
	    if (_res.dnsrch[i] == NULL) {
		break;
	    }
	    domain_search_count++;
	    if (debug) {
		if (i == 0) {
		    printf("DNS search:");
		}
		printf(" %s", _res.dnsrch[i]);
	    }
	}
	if (domain_search_count != 0) {
	    if (debug) {
		printf("\n");
	    }
	    S_domain_search 
		= DNSNameListBufferCreate((const char * *)_res.dnsrch,
					  domain_search_count,
					  NULL, &S_domain_search_size);
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
		strcpy(ip, inet_ntoa(info->addr));
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
    if (debug)
	inetroute_list_print(S_inetroutes);
}

static void
S_service_enable(CFTypeRef prop, u_int32_t which)
{
    int 	i;
    CFStringRef	ifname_cf = NULL;
    int		count;

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

#if !TARGET_OS_EMBEDDED
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

    if (S_which_services & service)
	return (TRUE);

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
    S_do_netboot = FALSE;
    S_do_old_netboot = FALSE;
    S_service_disable(SERVICE_NETBOOT | SERVICE_OLD_NETBOOT);
    return;
}
#endif /* !TARGET_OS_EMBEDDED */

typedef int (*qsort_compare_func_t)(const void *, const void *);

static struct ether_addr *
S_make_ether_list(CFArrayRef array, int * count_p)
{
    int			array_count = CFArrayGetCount(array);
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

static boolean_t
S_ok_to_respond(int hwtype, void * hwaddr, int hwlen)
{
    struct ether_addr *	search;
    boolean_t		respond = TRUE;

    if (hwlen != ETHER_ADDR_LEN) {
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
	    = (struct in_addr *)malloc(sizeof(struct in_addr *));
	S_relay_ip_list[0] = relay_ip;
	S_relay_ip_list_count = 1;
    }
    else {
	S_relay_ip_list_count++;
	S_relay_ip_list = (struct in_addr *)
	    realloc(S_relay_ip_list,
		    sizeof(struct in_addr *) * S_relay_ip_list_count);
	S_relay_ip_list[S_relay_ip_list_count - 1] = relay_ip;
    }
    return;
}

static void
S_update_relay_ip_list(CFArrayRef list)
{
    int		count;
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
	my_log(LOG_INFO, "Invalid '%s' property", prop_name);
    }
    return;
}

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

static void
S_update_services()
{
    uint32_t		num;
    CFDictionaryRef	plist = NULL;
    CFTypeRef		prop;

    plist = my_CFPropertyListCreateFromFile(BOOTPD_PLIST_PATH);
    if (plist != NULL) {
	if (isA_CFDictionary(plist) == NULL) {
	    CFRelease(plist);
	    plist = NULL;
	}
    }
    S_which_services = 0;

    if (plist != NULL) {
	/* BOOTP */
	S_service_enable(CFDictionaryGetValue(plist,
					      CFSTR(CFGPROP_BOOTP_ENABLED)),
			 SERVICE_BOOTP);
	
	/* DHCP */
	S_service_enable(CFDictionaryGetValue(plist,
					      CFSTR(CFGPROP_DHCP_ENABLED)),
			 SERVICE_DHCP);
#if !TARGET_OS_EMBEDDED
	/* NetBoot (2.0) */
	S_service_enable(CFDictionaryGetValue(plist,
					      CFSTR(CFGPROP_NETBOOT_ENABLED)),
			 SERVICE_NETBOOT);

	/* NetBoot (old, pre 2.0) */
	S_service_enable(CFDictionaryGetValue(plist,
					      CFSTR(CFGPROP_OLD_NETBOOT_ENABLED)),
			 SERVICE_OLD_NETBOOT);
#endif /* !TARGET_OS_EMBEDDED */
	/* Relay */
	S_service_enable(CFDictionaryGetValue(plist,
					      CFSTR(CFGPROP_RELAY_ENABLED)),
			 SERVICE_RELAY);
	prop = CFDictionaryGetValue(plist, CFSTR(CFGPROP_RELAY_IP_LIST));
	if (isA_CFArray(prop) != NULL) {
	    S_update_relay_ip_list(prop);
	}
    }
    /* allow/deny list */
    S_refresh_allow_deny(plist);

    /* reply threshold */
    reply_threshold_seconds = 0;
    set_number_from_plist(plist, CFSTR(CFGPROP_REPLY_THRESHOLD_SECONDS),
			  CFGPROP_REPLY_THRESHOLD_SECONDS,
			  &reply_threshold_seconds);

    /* detect other DHCP server */
    detect_other_dhcp_server = FALSE;
    num = 0;
    set_number_from_plist(plist, CFSTR(CFGPROP_DETECT_OTHER_DHCP_SERVER),
			  CFGPROP_DETECT_OTHER_DHCP_SERVER,
			  &num);
    if (num != 0) {
	detect_other_dhcp_server = TRUE;
    }

    /* ignore the DHCP client identifier */
    dhcp_ignore_client_identifier = FALSE;
    num = 0;
    set_number_from_plist(plist, CFSTR(CFGPROP_DHCP_IGNORE_CLIENT_IDENTIFIER),
			  CFGPROP_DHCP_IGNORE_CLIENT_IDENTIFIER,
			  &num);
    if (num != 0) {
	dhcp_ignore_client_identifier = TRUE;
    }
#if !TARGET_OS_EMBEDDED
    /* use open directory [for bootpent queries] */
    use_open_directory = TRUE;
    num = 1;
    set_number_from_plist(plist, CFSTR(CFGPROP_USE_OPEN_DIRECTORY),
			  CFGPROP_USE_OPEN_DIRECTORY,
			  &num);
    if (num == 0) {
	use_open_directory = FALSE;
    }
#endif /* !TARGET_OS_EMBEDDED */

    /* check whether to supply our own configuration for missing dhcp options */
    S_use_server_config_for_dhcp_options
	= S_get_plist_boolean(plist,
			      CFSTR(CFGPROP_USE_SERVER_CONFIG_FOR_DHCP_OPTIONS),
			      CFGPROP_USE_SERVER_CONFIG_FOR_DHCP_OPTIONS,
			      TRUE);

    /* get the new list of subnets */
    SubnetListFree(&subnets);
    if (plist != NULL) {
	prop = CFDictionaryGetValue(plist, BOOTPD_PLIST_SUBNETS);
	if (isA_CFArray(prop) != NULL) {
	    subnets = SubnetListCreateWithArray(prop);
	    if (subnets != NULL) {
		if (debug) {
		    SubnetListPrint(subnets);
		}
	    }
	}
    }

    dhcp_init();
#if !TARGET_OS_EMBEDDED
    if (S_do_netboot || S_do_old_netboot
	|| S_service_is_enabled(SERVICE_NETBOOT | SERVICE_OLD_NETBOOT)) {
	if (bsdp_init(plist) == FALSE) {
	    my_log(LOG_INFO, "bootpd: NetBoot service turned off");
	    S_disable_netboot();
	}
    }
#endif /* !TARGET_OS_EMBEDDED */
    if (plist != NULL) {
	CFRelease(plist);
    }
    return;
}

static __inline__ boolean_t
bootp_enabled(interface_t * if_p)
{
    u_int32_t 	which = (S_which_services | if_p->user_defined);

    return (S_do_bootp || (which & SERVICE_BOOTP) != 0);
}

static __inline__ boolean_t
dhcp_enabled(interface_t * if_p)
{
    u_int32_t 	which = (S_which_services | if_p->user_defined);

    return (S_do_dhcp || (which & SERVICE_DHCP) != 0);
}

#if !TARGET_OS_EMBEDDED
static __inline__ boolean_t
netboot_enabled(interface_t * if_p)
{
    u_int32_t 	which = (S_which_services | if_p->user_defined);

    return (S_do_netboot || (which & SERVICE_NETBOOT) != 0);
}

static __inline__ boolean_t
old_netboot_enabled(interface_t * if_p)
{
    u_int32_t 	which = (S_which_services | if_p->user_defined);

    return (S_do_old_netboot || (which & SERVICE_OLD_NETBOOT) != 0);
}
#endif /* !TARGET_OS_EMBEDDED */

static __inline__ boolean_t
relay_enabled(interface_t * if_p)
{
    u_int32_t 	which = (S_which_services | if_p->user_defined);

    return (S_do_relay || (which & SERVICE_RELAY) != 0);
}

void
usage()
{
    fprintf(stderr, "usage: bootpd <options>\n"
	    "<options> are:\n"
	    "[ -a ] 	support anonymous binding for BOOTP clients\n"
	    "[ -D ]	be a DHCP server\n"
	    "[ -B ]	don't service BOOTP requests\n"
	    "[ -b ] 	bootfile must exist or we don't respond\n"
	    "[ -d ]	debug mode, stay in foreground, extra printf's\n"
	    "[ -I ]	disable re-initialization on IP address changes\n"
	    "[ -i <interface> [ -i <interface> ... ] ]\n"
#if !TARGET_OS_EMBEDDED
	    "[ -m ] 	be an old NetBoot (1.0) server\n"
#endif /* !TARGET_OS_EMBEDDED */
	    "[ -n <domain> [ -n <domain> [...] ] ]\n"
#if !TARGET_OS_EMBEDDED
	    "[ -N ]	be a NetBoot 2.0 server\n"
#endif /* !TARGET_OS_EMBEDDED */
	    "[ -q ]	be quiet as possible\n"
	    "[ -r <server ip> [ -o <max hops> ] ] relay packets to server, "
	    "optionally set the hop count (default is 4 hops)\n"
	    "[ -v ] 	verbose mode, extra information\n"
	    );
    exit(1);
}

static void
S_add_ip_change_notifications();

int
main(int argc, char * argv[])
{
    int			ch;
    boolean_t		ip_change_notifications = TRUE;
    int			logopt = LOG_CONS;
    struct in_addr	relay_ip = { 0 };

    debug = 0;			/* no debugging ie. go into the background */
    verbose = 0;		/* don't print extra information */

    ptrlist_init(&S_if_list);

    S_get_interfaces();

    while ((ch =  getopt(argc, argv, "aBbc:DdhHi:I"
#if !TARGET_OS_EMBEDDED
        "mN"
#endif /* !TARGET_OS_EMBEDDED */
	"o:Pp:qr:St:v")) != EOF) {
	switch ((char)ch) {
	case 'a':
	    /* was: enable anonymous binding for BOOTP clients */
	    break;
	case 'B':
	    break;
	case 'S':
	    S_do_bootp = TRUE;
	    break;
	case 'b':
	    S_bootfile_noexist_reply = FALSE; 
	    /* reply only if bootfile exists */
	    break;
	case 'c':		    /* was: cache check interval - seconds */
	    break;
	case 'D':		/* answer DHCP requests as a DHCP server */
	    S_do_dhcp = TRUE;
	    break;
	case 'd':		/* stay in the foreground, extra printf's */
	    debug = 1;
	    break;
	case 'h':
	case 'H':
	    usage();
	    exit(1);
	    break;
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
#if !TARGET_OS_EMBEDDED
	case 'm':
	    S_do_old_netboot = TRUE;
	    S_do_dhcp = TRUE;
	    break;
	case 'N':
	    S_do_netboot = TRUE;
	    break;
#endif /* !TARGET_OS_EMBEDDED */
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
	    S_do_relay = 1;
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
	    verbose++;
	    break;
	default:
	    break;
	}
    }
    if (!issock(0)) { /* started by user */
	struct sockaddr_in Sin = { sizeof(Sin), AF_INET };
	int i;
	
	if (!debug)
	    background();
	
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
	    signal(SIGALRM, on_alarm);
	    alarm(15);
	}
    }

    writepid();

    if (debug)
	logopt = LOG_PERROR;

    (void) openlog("bootpd", logopt | LOG_PID, LOG_DAEMON);

    SubnetListLogErrors(LOG_NOTICE);

    my_log(LOG_DEBUG, "server starting");

    { 
	int opt = 1;

#if defined(IP_RECVIF)
	if (setsockopt(bootp_socket, IPPROTO_IP, IP_RECVIF, (caddr_t)&opt,
		       sizeof(opt)) < 0) {
	    my_log(LOG_INFO, "setsockopt(IP_RECVIF) failed: %s", 
		   strerror(errno));
	    exit(1);
	}
#endif
	
	if (setsockopt(bootp_socket, SOL_SOCKET, SO_BROADCAST, (caddr_t)&opt,
		       sizeof(opt)) < 0) {
	    my_log(LOG_INFO, "setsockopt(SO_BROADCAST) failed");
	    exit(1);
	}
	if (setsockopt(bootp_socket, IPPROTO_IP, IP_RECVDSTADDR, (caddr_t)&opt,
		       sizeof(opt)) < 0) {
	    my_log(LOG_INFO, "setsockopt(IPPROTO_IP, IP_RECVDSTADDR) failed");
	    exit(1);
	}
	if (setsockopt(bootp_socket, SOL_SOCKET, SO_REUSEADDR, (caddr_t)&opt,
		       sizeof(opt)) < 0) {
	    my_log(LOG_INFO, "setsockopt(SO_REUSEADDR) failed");
	    exit(1);
	}
    }
    
    /* install our sighup handler */
    signal(SIGHUP, on_sighup);

    if (ip_change_notifications) {
	S_add_ip_change_notifications();
    }
    S_server_loop();
    exit (0);
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
 * Function: on_sighup
 *
 * Purpose:
 *   If we get a sighup, re-read the subnet descriptions.
 */
static void
on_sighup(int sigraised)
{
    if (sigraised == SIGHUP)
	S_sighup = TRUE;
    return;
}

/*
 * Function: on_alarm
 *
 * Purpose:
 *   If we were started by inetd, we kill ourselves during periods of
 *   inactivity.  If we've been idle for MAXIDLE, exit.
 */
static void
on_alarm(int sigraised)
{
    struct timeval tv;
    
    gettimeofday(&tv, 0);
    
    if ((tv.tv_sec - S_lastmsgtime.tv_sec) >= MAXIDLE)
	exit(0);
    alarm(15);
    return;
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

    if (request_file && request_file[0])
	strcpy(file, request_file);
    else if (bootfile && bootfile[0])
	strcpy(file, bootfile);
    else {
	my_log(LOG_DEBUG, "no replyfile", path);
	return (TRUE);
    }

    if (file[0] == '/')	/* if absolute pathname */
	strcpy(path, file);
    else {
	strcpy(path, boot_tftp_dir);
	strcat(path, "/");
	strcat(path, file);
    }

    /* see if file exists with a ".host" suffix */
    if (hostname) {
	int 	n;

	n = strlen(path);
	strcat(path, ".");
	strcat(path, hostname);
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
    len = strlen(path);
    if (len >= reply_file_size) {
	my_log(LOG_DEBUG, "boot file name too long %d >= %d",
	       len, reply_file_size);
	return (TRUE);
    }
    my_log(LOG_DEBUG, "replyfile %s", path);
    strcpy(reply_file, path);
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

    if (giaddr.s_addr) { /* gateway'd */
	/* find a subnet entry on the same subnet as the gateway */
	if (subnets == NULL) {
	    return (FALSE);
	}
	return (SubnetListAreAddressesOnSameSupernet(subnets, ip, giaddr));
    }

    for (i = 0; i < S_inetroutes->count; i++) {
	inetroute_t * inr_p = S_inetroutes->list + i;

	if (inr_p->mask.s_addr != 0
	    && inr_p->gateway.link.sdl_family == AF_LINK
	    && (ifl_find_link(S_interfaces, inr_p->gateway.link.sdl_index) 
		== if_p)) {
	    /* reachable? */
	    if (in_subnet(inr_p->dest, inr_p->mask, ip))
		return (TRUE);
	}
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
	    printf("rq->bp_secs %d < threshold %d\n",
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
#if !TARGET_OS_EMBEDDED
	    if (use_open_directory == FALSE
	        || bootp_getbyhw_ds(rq->bp_htype, rq->bp_chaddr, rq->bp_hlen,
				    subnet_match, &match, &iaddr,
				    &hostname, &bootfile) == FALSE) {
		return;
	    }
#else /* TARGET_OS_EMBEDDED */
	    return;
#endif /* TARGET_OS_EMBEDDED */
	}
	rp.bp_yiaddr = iaddr;
    }
    else { /* client specified ip address */
	iaddr = rq->bp_ciaddr;
	if (bootp_getbyip_file(iaddr, &hostname, &bootfile) == FALSE) {
#if !TARGET_OS_EMBEDDED
	    if (use_open_directory == FALSE
	        || bootp_getbyip_ds(iaddr, &hostname, &bootfile) == FALSE) {
		return;
	    }
#else /* TARGET_OS_EMBEDDED */
	    return;
#endif /* TARGET_OS_EMBEDDED */
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
    strcpy((char *)rp.bp_sname, server_name);
    if (sendreply(request->if_p, &rp, sizeof(rp), FALSE, NULL)) {
	my_log(LOG_INFO, "reply sent %s %s pktsize %d",
	       hostname, inet_ntoa(iaddr), sizeof(rp));
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
		       bp->bp_hlen,
		       dst, if_inet_addr(if_p),
		       dest_port, src_port,
		       bp, n) < 0) {
	my_log(LOG_INFO, "transmit failed, %m");
	return (FALSE);
    }
    if (debug && verbose) {
	printf("\n=================== Server Reply ===="
	       "=================\n");
	dhcp_print_packet((struct dhcp *)bp, n);
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
    inet_addrinfo_t *	info = if_inet_addr_at(if_p, 0);
    static const uint8_t default_tags[] = { 
	dhcptag_subnet_mask_e, 
	dhcptag_router_e, 
	dhcptag_domain_name_server_e,
	dhcptag_domain_name_e,
	dhcptag_host_name_e,
    };
#define N_DEFAULT_TAGS	(sizeof(default_tags) / sizeof(default_tags[0]))
    int			number_before = dhcpoa_count(options);
    int			i;
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
			       strlen(hostname), hostname)
		    != dhcpoa_success_e) {
		    my_log(LOG_INFO, "couldn't add hostname: %s",
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
		    my_log(LOG_INFO, "couldn't add option %d: %s",
			   tags[i], dhcpoa_err(options));
		}
	    }
	}
	if (handled == FALSE && S_use_server_config_for_dhcp_options) {
	    /* try to use defaults if no explicit configuration */
	    struct in_addr * def_route;

	    switch (tags[i]) {
	      case dhcptag_subnet_mask_e: {
		if (ifl_find_subnet(S_interfaces, iaddr) != if_p)
		    continue;
		if (dhcpoa_add(options, dhcptag_subnet_mask_e, 
			       sizeof(info->mask), &info->mask) 
		    != dhcpoa_success_e) {
		    my_log(LOG_INFO, "couldn't add subnet_mask: %s",
			   dhcpoa_err(options));
		    continue;
		}
		my_log(LOG_DEBUG, "subnet mask %s derived from %s",
		       inet_ntoa(info->mask), if_name(if_p));
		break;
	      }
	      case dhcptag_router_e:
		def_route = inetroute_default(S_inetroutes);
		if (def_route == NULL
		    || in_subnet(info->netaddr, info->mask,
				 *def_route) == FALSE
		    || in_subnet(info->netaddr, info->mask,
				 iaddr) == FALSE)
		    /* don't respond if default route not on same subnet */
		    continue;
		if (dhcpoa_add(options, dhcptag_router_e, sizeof(*def_route),
			       def_route) != dhcpoa_success_e) {
		    my_log(LOG_INFO, "couldn't add router: %s",
			   dhcpoa_err(options));
		    continue;
		}
		my_log(LOG_DEBUG, "default route added as router");
		break;
	      case dhcptag_domain_name_server_e:
		if (S_dns_servers_count == 0)
		    continue;
		if (dhcpoa_add(options, dhcptag_domain_name_server_e,
			       S_dns_servers_count * sizeof(*S_dns_servers),
			       S_dns_servers) != dhcpoa_success_e) {
		    my_log(LOG_INFO, "couldn't add dns servers: %s",
			   dhcpoa_err(options));
		    continue;
		}
		if (verbose)
		    my_log(LOG_DEBUG, "default dns servers added");
		break;
	      case dhcptag_domain_name_e:
		if (S_domain_name) {
		    if (dhcpoa_add(options, dhcptag_domain_name_e,
				   strlen(S_domain_name), S_domain_name)
			!= dhcpoa_success_e) {
			my_log(LOG_INFO, "couldn't add domain name: %s",
			       dhcpoa_err(options));
			continue;
		    }
		    if (verbose)
			my_log(LOG_DEBUG, "default domain name added");
		}
		break;
	      case dhcptag_domain_search_e:
		if (S_domain_search) {
		    if (dhcpoa_add(options, dhcptag_domain_search_e,
				   S_domain_search_size, S_domain_search)
			!= dhcpoa_success_e) {
			my_log(LOG_INFO, "couldn't add domain search: %s",
			       dhcpoa_err(options));
			continue;
		    }
		    if (verbose)
			my_log(LOG_DEBUG, "domain search added");
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
	    if (debug && verbose && printed == FALSE) {
		printed = TRUE;
		printf("\n=================== Relayed Request ===="
		       "=================\n");
		dhcp_print_packet((struct dhcp *)bp, n);
	    }

	    if (bootp_transmit(bootp_socket, transmit_buffer, if_name(if_p),
			       bp->bp_htype, NULL, 0, 
			       relay, if_inet_addr(if_p),
			       S_ipport_server, S_ipport_client,
			       bp, n) < 0) {
		my_log(LOG_INFO, "send to %s failed, %m", inet_ntoa(relay));
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
	if (debug && verbose) {
	    if (debug) {
		printf("\n=================== Relayed Reply ===="
		       "=================\n");
		dhcp_print_packet((struct dhcp *)bp, n);
	    }
	}
	if (bootp_transmit(bootp_socket, transmit_buffer, if_name(if_p),
			   bp->bp_htype, bp->bp_chaddr, bp->bp_hlen,
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

static void
S_dispatch_packet(struct bootp * bp, int n, interface_t * if_p,
		  struct in_addr * dstaddr_p)
{
#if !TARGET_OS_EMBEDDED
    boolean_t		bsdp_pkt = FALSE;
#endif /* !TARGET_OS_EMBEDDED */
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
	
	if (debug && verbose) {
	    printf("\n---------------- Client Request --------------------\n");
	    dhcp_print_packet((struct dhcp *)bp, n);
	}

	if (bp->bp_sname[0] != '\0' 
	    && strcmp((char *)bp->bp_sname, server_name) != 0)
	    goto request_done;

	if (bp->bp_siaddr.s_addr != 0
	    && ntohl(bp->bp_siaddr.s_addr) != ntohl(if_inet_addr(if_p).s_addr))
	    goto request_done;

	if (dhcp_pkt) { /* this is a DHCP packet */
#if !TARGET_OS_EMBEDDED
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
#endif /* !TARGET_OS_EMBEDDED */
	    if (dhcp_enabled(if_p)
#if !TARGET_OS_EMBEDDED
	        || old_netboot_enabled(if_p)
#endif /* !TARGET_OS_EMBEDDED */
	        ) {
		handled = TRUE;
		dhcp_request(&request, dhcp_msgtype, dhcp_enabled(if_p));
	    }
	}
#if !TARGET_OS_EMBEDDED
	if (handled == FALSE && old_netboot_enabled(if_p)) {
	    handled = old_netboot_request(&request);
	}
#endif /* !TARGET_OS_EMBEDDED */
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
	S_relay_packet((struct bootp *)S_rxpkt, n, if_p);
    }

    if (verbose || debug) {
	struct timeval now;
	struct timeval result;

	gettimeofday(&now, 0);
	timeval_subtract(now, S_lastmsgtime, &result);
	my_log(LOG_INFO, "service time %d.%06d seconds",
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

#if defined(IP_RECVIF)
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
    if (if_inet_valid(if_p) == FALSE)
	return (NULL);
    if (ptrlist_count(&S_if_list) > 0
	&& S_string_in_list(&S_if_list, ifname) == FALSE) {
	if (verbose)
	    my_log(LOG_DEBUG, "ignoring request on %s", ifname);
	return (NULL);
    }
    return (if_p);
}
#endif

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
 * Function: S_server_loop
 *
 * Purpose:
 *   This is the main loop that dispatches a request according to
 *   whether it is BOOTP, DHCP, or NetBoot.
 */
static void
S_server_loop()
{
    struct in_addr * 	dstaddr_p = NULL;
    struct sockaddr_in 	from = { sizeof(from), AF_INET };
    interface_t *	if_p = NULL;
    int 		mask;
    int			n;
    struct dhcp *	request = (struct dhcp *)S_rxpkt;

    for (;;) {
	S_init_msg();
	msg.msg_name = (caddr_t)&from;
	msg.msg_namelen = sizeof(from);
	n = recvmsg(bootp_socket, &msg, 0);
	if (n < 0) {
	    my_log(LOG_DEBUG, "recvmsg failed, %m");
	    errno = 0;
	    continue;
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
	    S_update_services();
	    S_get_dns();
	    S_sighup = FALSE;
	}

	if (n < sizeof(struct dhcp)) {
	    continue;
	}
	if (request->dp_hlen > sizeof(request->dp_chaddr)) {
	    continue;
	}
	if (S_ok_to_respond(request->dp_htype, request->dp_chaddr, 
			    request->dp_hlen) == FALSE) {
	    continue;
	}
	dstaddr_p = S_which_dstaddr();
	if (debug) {
	    if (dstaddr_p == NULL)
		printf("no destination address\n");
	    else
		printf("destination address %s\n", inet_ntoa(*dstaddr_p));
	}

#if defined(IP_RECVIF)
	if_p = S_which_interface();
	if (if_p == NULL) {
	    continue;
	}
#else 
	if_p = if_first_broadcast_inet(S_interfaces);
#endif

	gettimeofday(&S_lastmsgtime, 0);
        mask = sigblock(sigmask(SIGALRM));
	S_dispatch_packet((struct bootp *)S_rxpkt, n, if_p, dstaddr_p);
	sigsetmask(mask);
    }
    return;
}

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>
static SCDynamicStoreRef	store;
static void
S_add_ip_change_notifications()
{
    CFStringRef			key;
    CFMutableArrayRef		patterns;

    store = SCDynamicStoreCreate(NULL,
				 CFSTR("com.apple.network.bootpd"),
				 NULL,
				 NULL);
    if (store == NULL) {
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
    SCDynamicStoreNotifySignal(store, getpid(), SIGHUP);
    return;
}
