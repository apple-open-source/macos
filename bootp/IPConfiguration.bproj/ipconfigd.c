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
 */

#import <stdlib.h>
#import <unistd.h>
#import <string.h>
#import <stdio.h>
#import <sys/types.h>
#import <sys/wait.h>
#import <sys/errno.h>
#import <sys/socket.h>
#define KERNEL_PRIVATE
#import <sys/ioctl.h>
#undef KERNEL_PRIVATE
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
#import <net/if_types.h>
#import <net/if_media.h>

#import <SystemConfiguration/SystemConfiguration.h>
#import <SystemConfiguration/SCPrivate.h>
#import <SystemConfiguration/SCValidation.h>
#import <SystemConfiguration/SCDPlugin.h>

#import "rfc_options.h"
#import "dhcp_options.h"
#import "dhcp.h"
#import "interfaces.h"
#import "util.h"
#import "arp.h"

#import "host_identifier.h"
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


#define kDHCPClientPreferencesID	CFSTR("DHCPClient.xml")
#define kDHCPClientApplicationPref	CFSTR("Application")
#define kDHCPRequestedParameterList	CFSTR("DHCPRequestedParameterList")

#ifndef kSCEntNetDHCP
#define kSCEntNetDHCP			CFSTR("DHCP")
#endif kSCEntNetDHCP

#define USE_FLAT_FILES			"UseFlatFiles"

#define USER_ERROR			1
#define UNEXPECTED_ERROR 		2
#define TIMEOUT_ERROR			3

/* global variables */
u_short 			G_client_port = IPPORT_BOOTPC;
boolean_t 			G_dhcp_accepts_bootp = FALSE;
u_short 			G_server_port = IPPORT_BOOTPS;
u_long				G_link_inactive_secs = LINK_INACTIVE_WAIT_SECS;
u_long				G_gather_secs = GATHER_TIME_SECS;
u_long				G_max_retries = MAX_RETRIES;
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
static ptrlist_t		S_ifstate_list;
static interface_list_t	*	S_interfaces = NULL;
static SCDynamicStoreRef	S_scd_session = NULL;
static CFStringRef		S_setup_service_prefix = NULL;
static CFStringRef		S_state_interface_prefix = NULL;
static char * 			S_computer_name = NULL;
static CFStringRef		S_computer_name_key = NULL;
static CFStringRef		S_dhcp_preferences_key = NULL;
static boolean_t		S_verbose = FALSE;

#define PROP_SERVICEID		CFSTR("ServiceID")


static void
S_add_dhcp_parameters();

static void
configuration_changed(SCDynamicStoreRef session);

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
    CFIndex		namelen;
    CFRange		r;
    CFIndex		used_len = 0;

    if (session == NULL)
	return;

    if (S_computer_name) {
	free(S_computer_name);
	S_computer_name = NULL;
    }

    name = SCDynamicStoreCopyComputerName(session, &encoding);
    if (name == NULL) {
	return;
    }
    namelen = CFStringGetLength(name);
    if (namelen == 0) {
	goto done;
    }
    r = CFRangeMake(0, namelen);
    if (CFStringGetBytes(name, r, encoding, '.', FALSE, buf, sizeof(buf) - 1, 
			 &used_len) > 0) {
	buf[used_len] = '\0';
	S_computer_name = strdup(buf);
    }
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

static void *
read_file(char * filename, size_t * data_length)
{
    void *		data = NULL;
    size_t		len = 0;
    int			fd = -1;
    struct stat		sb;

    *data_length = 0;
    if (stat(filename, &sb) < 0)
	goto done;
    len = sb.st_size;
    if (len == 0)
	goto done;

    data = malloc(len);
    if (data == NULL)
	goto done;

    fd = open(filename, O_RDONLY);
    if (fd < 0)
	goto done;

    if (read(fd, data, len) != len) {
	goto done;
    }
 done:
    if (fd >= 0)
	close(fd);
    if (data) {
	*data_length = len;
    }
    return (data);
}

static void
write_file(char * filename, void * data, size_t data_length)
{
    char		path[MAXPATHLEN];
    int			fd = -1;

    snprintf(path, sizeof(path), "%s-", filename);
    fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd < 0) {
	my_log(LOG_INFO, "open(%s) failed, %s", filename, strerror(errno));
	goto done;
    }

    if (write(fd, data, data_length) != data_length) {
	my_log(LOG_INFO, "write(%s) failed, %s", filename, strerror(errno));
	goto done;
    }
    rename(path, filename);
 done:
    if (fd >= 0) {
	close(fd);
    }
    return;
}

static CFPropertyListRef 
propertyListRead(char * filename)
{
    void *		buf;
    size_t		bufsize;
    CFDataRef		data = NULL;
    CFPropertyListRef	plist = NULL;

    buf = read_file(filename, &bufsize);
    if (buf == NULL) {
	return (NULL);
    }
    data = CFDataCreate(NULL, buf, bufsize);
    if (data == NULL) {
	goto error;
    }

    plist = CFPropertyListCreateFromXMLData(NULL, data, 
					    kCFPropertyListImmutable,
					    NULL);
 error:
    if (data)
	CFRelease(data);
    if (buf)
	free(buf);
    return (plist);
}

static void
propertyListWrite(char * filename, CFPropertyListRef plist)
{
    CFDataRef	data;

    if (plist == NULL)
	return;

    data = CFPropertyListCreateXMLData(NULL, plist);
    if (data == NULL) {
	return;
    }
    write_file(filename, (void *)CFDataGetBytePtr(data), CFDataGetLength(data));
    CFRelease(data);
    return;
}

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
    dict = propertyListRead(filename);
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
    propertyListWrite(filename, dict);
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
    u_long m = ntohl(mask.s_addr);
    u_long val1 = ntohl(ip1.s_addr);
    u_long val2 = ntohl(ip2.s_addr);

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
	ifra.ifra_mask = G_blank_sin;
	((struct sockaddr_in *)&ifra.ifra_broadaddr)->sin_addr = *broadcast;
    }
    return (ioctl(s, SIOCAIFADDR, &ifra));
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
	    my_log(LOG_ERR, "make_ifstate: malloc ifstate failed");
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
    if (S_scd_session != NULL && ifstate->serviceID) {
	CFMutableArrayRef	array;
	CFStringRef		key;

	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (array == NULL) {
	    return;
	}

	/* ipv4 */
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  ifstate->serviceID,
							  kSCEntNetIPv4);
	if (key) {
	    CFArrayAppendValue(array, key);
	}
	my_CFRelease(&key);

	/* dns */
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  ifstate->serviceID,
							  kSCEntNetDNS);
	if (key) {
	    CFArrayAppendValue(array, key);
	}
	my_CFRelease(&key);

	/* netinfo */
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  ifstate->serviceID,
							  kSCEntNetNetInfo);
	if (key) {
	    CFArrayAppendValue(array, key);
	}
	my_CFRelease(&key);

	/* dhcp */
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  ifstate->serviceID,
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
make_dhcp_dict(IFState_t * ifstate)
{
    CFMutableDictionaryRef	dict;
    struct completion_results *	pub;
    int				tag;

    pub = &ifstate->published;
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

    if (ifstate->method == ipconfig_method_dhcp_e) {
	CFDateRef	start;
	
	start = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
	CFDictionarySetValue(dict, CFSTR("LeaseStartTime"), start);
	my_CFRelease(&start);
    }

    return (dict);
}

void
ifstate_publish_success(IFState_t * ifstate, void * pkt, int pkt_size)
{
    int				address_count = 0;
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
    CFMutableDictionaryRef	ipv4_dict = NULL;
    CFMutableDictionaryRef	netinfo_dict = NULL;
    struct in_addr *		netinfo_addresses = NULL;
    int				netinfo_addresses_len = 0;
    u_char *			netinfo_tag = NULL;
    int				netinfo_tag_len = 0;
    struct completion_results *	pub;
    struct in_addr *		router = NULL;
    int				router_len = 0;

    if (ifstate->serviceID == NULL) {
	return;
    }

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
	address_count++;
	CFRelease(str);
    }
    CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Addresses, array);
    CFRelease(array);

    /* set the ip mask array */
    array = CFArrayCreateMutable(NULL, if_inet_count(ifstate->if_p), 
				 &kCFTypeArrayCallBacks);
    for (i = ifstate->our_addrs_start; i < if_inet_count(ifstate->if_p); i++) {
	inet_addrinfo_t * 	if_inet = if_inet_addr_at(ifstate->if_p, i);
	CFStringRef		str;

	if (if_inet->addr.s_addr == 0)
	    continue;

	str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), 
				       IP_LIST(&if_inet->mask));
	CFArrayAppendValue(array, str);
	CFRelease(str);
    }
    CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4SubnetMasks, array);
    CFRelease(array);

    CFDictionarySetValue(ipv4_dict, CFSTR("InterfaceName"),
			 ifstate->ifname);

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
    dhcp_dict = make_dhcp_dict(ifstate);

    publish_service(ifstate->serviceID, ipv4_dict, dns_dict,
		    netinfo_dict, dhcp_dict);

    my_CFRelease(&ipv4_dict);
    my_CFRelease(&dns_dict);
    my_CFRelease(&netinfo_dict);
    my_CFRelease(&dhcp_dict);

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

void
ifstate_remove_addresses(IFState_t * ifstate)
{
    inet_addrinfo_t * info;

    while (info = if_inet_addr_at(ifstate->if_p, 
				  ifstate->our_addrs_start)) {
	my_log(LOG_DEBUG, "%s: removing %s",
	       if_name(ifstate->if_p), inet_ntoa(info->addr));
	inet_remove(ifstate, info->addr);
    }
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
inet_set_autoaddr(IFState_t * ifstate, int val)
{
    interface_t *	if_p = ifstate->if_p;
    int 		s = inet_dgram_socket();
    int			ret = 0;

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "inet_set_autoaddr(%s, %d): socket() failed, %s (%d)",
	       if_name(if_p), val, strerror(errno), errno);
    }
    else {
	if (siocautoaddr(s, if_name(if_p), val) < 0) {
	    ret = errno;
	    my_log(LOG_DEBUG, "inet_set_autoaddr(%s, %d) failed, %s (%d)", 
		   if_name(if_p), val, strerror(errno), errno);

	}
	close(s);
    }
    return (ret);
}

int
inet_enable_autoaddr(IFState_t * ifstate)
{
    return (inet_set_autoaddr(ifstate, 1));
}

int
inet_disable_autoaddr(IFState_t * ifstate)
{
    arpcache_flush(G_ip_zeroes, G_ip_zeroes);
    return (inet_set_autoaddr(ifstate, 0));
}

int
inet_attach_interface(IFState_t * ifstate)
{
    int ret = 0;
    int s = inet_dgram_socket();

    if (s < 0) {
	ret = errno;
	goto done;
    }

    if (siocprotoattach(s, if_name(ifstate->if_p)) < 0) {
	ret = errno;
	my_log(LOG_DEBUG, "siocprotoattach(%s) failed, %s (%d)", 
	       if_name(ifstate->if_p), strerror(errno), errno);
    }
    (void)ifflags_set(s, if_name(ifstate->if_p), IFF_UP);
    close(s);

 done:
    return (ret);
}

static int
inet_detach_interface(IFState_t * ifstate)
{
    int ret = 0;
    int s = inet_dgram_socket();

    if (s < 0) {
	ret = errno;
	goto done;
    }
    if (siocprotodetach(s, if_name(ifstate->if_p)) < 0) {
	ret = errno;
	my_log(LOG_DEBUG, "siocprotodetach(%s) failed, %s (%d)", 
	       if_name(ifstate->if_p), strerror(errno), errno);
    }
    close(s);

 done:
    return (ret);
}

static boolean_t
subnet_route(int cmd, struct in_addr netaddr, struct in_addr netmask,
	     char * ifname)
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
	my_log(LOG_DEBUG, "subnet_route: write routing socket failed, %s",
	       strerror(errno));
	ret = FALSE;
    }
 done:
    if (sockfd >= 0) {
	close(sockfd);
    }
    return (ret);
}

#define RANK_LOWEST	(1024 * 1024)
static unsigned int
S_get_service_rank(CFArrayRef arr, CFStringRef serviceID)
{
    int i;

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

static int
S_ifstate_find_netaddr(IFState_t * ifstate, struct in_addr netaddr)
{
    int 		i;
    interface_t * 	if_p = ifstate->if_p;

    for (i = ifstate->our_addrs_start; i < if_inet_count(if_p); i++) {
	inet_addrinfo_t *	info = if_inet_addr_at(if_p, i);

	if (netaddr.s_addr == info->netaddr.s_addr) {
	    return (i);
	}
    }
    return (INDEX_BAD);
}

static int
S_ifstate_first_addr(IFState_t * ifstate)
{
    int 		i;
    interface_t * 	if_p = ifstate->if_p;

    for (i = ifstate->our_addrs_start; i < if_inet_count(if_p); i++) {
	inet_addrinfo_t *	info = if_inet_addr_at(if_p, i);
	u_int32_t		ipval;

	ipval = iptohl(info->netaddr);
	if (IN_LINKLOCAL(ipval)) {
	    continue;
	}
	return (i);
    }
    return (INDEX_BAD);
}

boolean_t
S_subnet_route_interface(struct in_addr netaddr, struct in_addr netmask,
			 interface_t * * if_p_p, int * count_p)
{
    unsigned int	best_rank = RANK_LOWEST;
    interface_t *	best_if_p = NULL;
    int			match_count = 0;
    int 		i;
    boolean_t		ret = TRUE;
    CFArrayRef		service_order = NULL;

    if (S_scd_session == NULL 
	|| (service_order = S_get_service_order(S_scd_session)) == NULL) {
	ret = FALSE;
	goto done;
    }

    for (i = 0; i < ptrlist_count(&S_ifstate_list); i++) {
	int		addr_index;
	unsigned int	rank;
	IFState_t *	ifstate = ptrlist_element(&S_ifstate_list, i);
	interface_t *	if_p = ifstate->if_p;
	
	if (ifstate->method == ipconfig_method_none_e
	    || ifstate->serviceID == NULL) {
	    continue;
	}
	addr_index = S_ifstate_find_netaddr(ifstate, netaddr);
	if (addr_index == INDEX_BAD) {
	    continue;
	}
	match_count++;
	rank = S_get_service_rank(service_order, ifstate->serviceID);
	if (rank < best_rank) {
	    best_if_p = if_p;
	    best_rank = rank;
	}
    }
 done:
    my_CFRelease(&service_order);
    if (count_p) {
	*count_p = match_count;
    }
    if (if_p_p) {
	*if_p_p = best_if_p;
    }
    return (ret);
}

static void
order_services(SCDynamicStoreRef session)
{
    int i;

    for (i = 0; i < ptrlist_count(&S_ifstate_list); i++) {
	int			addr_index;
	interface_t *		best_if_p = NULL;
	IFState_t *		ifstate = ptrlist_element(&S_ifstate_list, i);
	inet_addrinfo_t *	info;
	interface_t *		if_p = ifstate->if_p;
	int			subnet_count = 0;

	if (ifstate->method == ipconfig_method_none_e
	    || ifstate->serviceID == NULL) {
	    continue;
	}
	addr_index = S_ifstate_first_addr(ifstate);
	if (addr_index == INDEX_BAD) {
	    continue;
	}
	info = if_inet_addr_at(if_p, addr_index);
	if (S_subnet_route_interface(info->netaddr, info->mask,
				     &best_if_p, &subnet_count) == TRUE) {
	    /* adjust the subnet route if more than one if is on it */
	    if (best_if_p && subnet_count > 1) {
		subnet_route(RTM_DELETE, info->netaddr, info->mask, NULL);
		subnet_route(RTM_ADD, info->netaddr, info->mask, 
			     if_name(best_if_p));
		arpcache_flush(G_ip_zeroes, info->broadcast);
	    }
	}
    }
    return;
}

int
inet_add(IFState_t * ifstate, struct in_addr ip, 
	 const struct in_addr * mask, const struct in_addr * broadcast)
{
    interface_t *	if_p = ifstate->if_p;
    struct in_addr	netmask = { 0 };
    struct in_addr	netaddr = { 0 };
    struct in_addr	netbroadcast = { 0 };
    int			ret = 0;
    int 		s = inet_dgram_socket();

    if (mask == NULL || mask->s_addr == 0 
	|| mask->s_addr == G_ip_broadcast.s_addr) {
	u_int32_t ipval = ntohl(ip.s_addr);

	if (IN_CLASSA(ipval)) {
	    netmask.s_addr = htonl(IN_CLASSA_NET);
	}
	else if (IN_CLASSB(ipval)) {
	    netmask.s_addr = htonl(IN_CLASSB_NET);
	}
	else {
	    netmask.s_addr = htonl(IN_CLASSC_NET);
	}
    }
    else {
	netmask = *mask;
    }

    my_log(LOG_DEBUG, "inet_add: %s " IP_FORMAT " netmask " IP_FORMAT, 
	   if_name(if_p), IP_LIST(&ip), IP_LIST(&netmask));
    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "inet_add(%s): socket() failed, %s (%d)",
	       if_name(if_p), strerror(errno), errno);
    }
    else {
	int			i;
	inet_addrinfo_t 	info;

	if (inet_aifaddr(s, if_name(if_p), &ip, &netmask, broadcast) < 0) {
	    ret = errno;
	    my_log(LOG_DEBUG, "inet_add(%s) " 
		   IP_FORMAT " inet_aifaddr() failed, %s (%d)", if_name(if_p),
		   IP_LIST(&ip), strerror(errno), errno);
	}
	if_setflags(if_p, if_flags(if_p) | IFF_UP);
	ifflags_set(s, if_name(if_p), IFF_UP);
	bzero(&info, sizeof(info));
	info.addr = ip;
	info.mask = netmask;
	info.netaddr = netaddr = hltoip(iptohl(ip) & iptohl(netmask));
	if (broadcast) {
	    info.broadcast = *broadcast;
	}
	else {
	    info.broadcast = hltoip(iptohl(ip) | ~iptohl(netmask));
	}
	netbroadcast = info.broadcast;
	i = if_inet_find_ip(if_p, info.addr);
	if (i != INDEX_BAD) {
	    if (i < ifstate->our_addrs_start) {
		ifstate->our_addrs_start--;
	    }
	}
	if_inet_addr_add(if_p, &info);
	close(s);
    }
    if (ret == EEXIST) {
	interface_t * 	if_p = NULL;

	if (S_subnet_route_interface(netaddr, netmask, &if_p, NULL) == TRUE) {
	    if (if_p) {
		subnet_route(RTM_DELETE, netaddr, netmask, NULL);
		subnet_route(RTM_ADD, netaddr, netmask, if_name(if_p));
	    }
	    else {
		subnet_route(RTM_DELETE, netaddr, netmask, NULL);
	    }
	}
	arpcache_flush(G_ip_zeroes, netbroadcast);
    }
    return (ret);
}

int
inet_remove(IFState_t * ifstate, struct in_addr ip)
{
    interface_t *	if_p = ifstate->if_p;
    struct in_addr	netmask = { 0 };
    struct in_addr	netaddr = { 0 };
    int			ret = 0;
    int 		s = inet_dgram_socket();

    my_log(LOG_DEBUG, "inet_remove: %s " IP_FORMAT, 
	   if_name(if_p), IP_LIST(&ip));
    if (s < 0) {
	ret = errno;
	my_log(LOG_DEBUG, 
	       "inet_remove (%s):socket() failed, %s (%d)",
	       if_name(if_p), strerror(errno), errno);
    }	
    else {
	int		i;
	struct in_addr	broadcast = G_ip_zeroes;

	i = if_inet_find_ip(if_p, ip);
	if (i != INDEX_BAD) {
	    inet_addrinfo_t *	info = if_inet_addr_at(if_p, i);

	    netmask = info->mask;
	    netaddr = info->netaddr;
	    broadcast = info->broadcast;
	}
	if (inet_difaddr(s, if_name(if_p), &ip) < 0) {
	    ret = errno;
	    my_log(LOG_DEBUG, "inet_remove(%s) " 
		   IP_FORMAT " failed, %s (%d)", if_name(if_p),
		   IP_LIST(&ip), strerror(errno), errno);
	}
	if_inet_addr_remove(if_p, ip);
	close(s);
	arpcache_flush(ip, broadcast);
    }
    if (ip.s_addr != 0 && netmask.s_addr != 0) {
	interface_t * 	if_p = NULL;

	if (S_subnet_route_interface(netaddr, netmask, &if_p, NULL) == TRUE) {
	    if (if_p) {
		subnet_route(RTM_DELETE, netaddr, netmask, NULL);
		subnet_route(RTM_ADD, netaddr, netmask, if_name(if_p));
	    }
	    else {
		subnet_route(RTM_DELETE, netaddr, netmask, NULL);
	    }
	}
    }
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
    ipconfig_status_t	status = ipconfig_status_success_e;
    ipconfig_func_t *	func;

    if (method == ipconfig_method_none_e) {
	goto done;
    }

    func = lookup_func(method);
    if (func == NULL) {
	status = ipconfig_status_internal_error_e;
	goto done;
    }
    (*func)(ifstate, IFEventID_stop_e, NULL);

 done:
    return (status);
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

static ipconfig_status_t
config_method_renew(IFState_t * ifstate)
{
    ipconfig_func_t *	func;

    if (ifstate->method == ipconfig_method_none_e) {
	return (ipconfig_status_success_e);
    }
    func = lookup_func(ifstate->method);
    if (func == NULL) {
	return (ipconfig_status_internal_error_e);
    }
    (*func)(ifstate, IFEventID_renew_e, NULL);
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

    if (G_verbose)
	my_log(LOG_INFO, "set %s %s", name, ipconfig_method_string(method));
    if (if_p == NULL) {
	my_log(LOG_INFO, "set: unknown interface %s", name);
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
    my_CFRelease(&ifstate->serviceID);

    if (method == ipconfig_method_none_e) {
	inet_detach_interface(ifstate);
    }
    else {
	inet_attach_interface(ifstate);
	status = config_method_start(ifstate, method,
				     method_data, method_data_len);
	my_log(LOG_DEBUG, "status from %s was %s",
	       ipconfig_method_string(method), 
	       ipconfig_status_string(status));
	if (status == ipconfig_status_success_e) {
	    ifstate->method = method;
	    if (serviceID) {
		ifstate->serviceID = (void *)CFRetain(serviceID);
	    }
	    else {
		ifstate->serviceID = (void *)
		    CFStringCreateWithFormat(NULL, NULL, 
					     CFSTR("%s-%s"),
					     if_name(if_p),
					     ipconfig_method_string(method));
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
    CFDictionaryGetKeysAndValues(values, keys, NULL);
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

static void
handle_configuration_changed(SCDynamicStoreRef session,
			     CFArrayRef all_ipv4)
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
	ipv4_dict = lookup_entity(all_ipv4, ifn_cf);
	if (ipv4_dict == NULL) {
	    status = set_if(if_name(if_p), ipconfig_method_none_e, NULL, 0,
			    NULL);
	    if (status != ipconfig_status_success_e) {
		my_log(LOG_INFO, "ipconfigd: set %s %s failed, %s",
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
		    my_log(LOG_INFO, "ipconfigd: set %s %s failed, %s",
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
		    my_log(LOG_INFO, "ipconfigd: set %s %s failed, %s",
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

static boolean_t
start_initialization(SCDynamicStoreRef session)
{
    CFStringRef		key;
    CFPropertyListRef	value = NULL;

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
	syslog(LOG_INFO, "IPConfiguration needs PreferencesMonitor to run first");
    }
    my_CFRelease(&value);

    /* install run-time notifiers */
    notifier_init(session);

    (void)update_interface_list();

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

ipconfig_status_t
state_key_changed(SCDynamicStoreRef session, CFStringRef cache_key)
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
    SCLog(G_verbose, LOG_INFO, CFSTR("%s link is %s"),
	  ifn, link.active ? "up" : "down");
    istatus = config_method_media(ifstate, link);

 done:
    my_CFRelease(&ifn_cf);
    return (istatus);
}

static void
dhcp_preferences_changed(SCDynamicStoreRef session)
{
    int i;

    /* merge in the new requested parameters */
    S_add_dhcp_parameters();

    for (i = 0; i < ptrlist_count(&S_ifstate_list); i++) {
	IFState_t *	element = ptrlist_element(&S_ifstate_list, i);

	/* ask each interface to renew immediately to pick up new options */
	config_method_renew(element);
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
    /* an interface was added */
    if (iflist_changed) {
	config_changed = update_interface_list();
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

	if (CFStringHasPrefix(cache_key, kSCDynamicStoreDomainState)) {
	    (void)state_key_changed(session, cache_key);
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
    ptrlist_init(&S_ifstate_list);
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
    plist = propertyListRead(path);
    if (plist && isA_CFDictionary(plist)) {
	u_char *	dhcp_params = NULL;
	int		n_dhcp_params = 0;

	G_dhcp_accepts_bootp 
	    = S_get_plist_boolean(plist, CFSTR("DHCPAcceptsBOOTP"), FALSE);
	G_verbose = S_get_plist_boolean(plist, CFSTR("Verbose"), FALSE);
	G_must_broadcast 
	    = S_get_plist_boolean(plist, CFSTR("MustBroadcast"), FALSE);
	G_max_retries = S_get_plist_int(plist, CFSTR("RetryCount"), 2);
	G_gather_secs = S_get_plist_int(plist, CFSTR("GatherTimeSeconds"), 2);
	G_link_inactive_secs 
	    = S_get_plist_int(plist, CFSTR("LinkInactiveWaitTimeSeconds"), 4);
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

    G_arp_session = arp_session_init(S_is_our_hardware_address);
    if (G_arp_session == NULL) {
	my_log(LOG_DEBUG, "arp_session_init() failed");
	return;
    }

    bootp_session_set_debug(G_bootp_session, G_debug);
    ptrlist_init(&S_ifstate_list);

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
