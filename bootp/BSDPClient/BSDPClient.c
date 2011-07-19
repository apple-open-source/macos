/*
 * Copyright (c) 2002-2004, 2010 Apple Inc. All rights reserved.
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
 * BSDPClient.c
 * - BSDP client library functions
 */

/* 
 * Modification History
 *
 * February 25, 2002	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <ctype.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/bootp.h>
#include <arpa/inet.h>

#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFSocket.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#include "ioregpath.h"
#include "BSDPClient.h"
#include "BSDPClientPrivate.h"
#include "rfc_options.h"
#include "dhcp_options.h"
#include "bsdp.h"
#include "util.h"
#include "cfutil.h"
#include "dhcplib.h"
#include "interfaces.h"
#include "bootp_transmit.h"

#define BSDPCLIENT_MAX_WAIT_SECS		16
#define BSDPCLIENT_LIST_MAX_TRIES		7
#define BSDPCLIENT_SELECT_MAX_TRIES		2
#define BSDPCLIENT_INITIAL_TIMEOUT_SECS		2

#ifdef TEST_BAD_SYSID
const uint8_t	bad_sysid1[] = {
    'A', 'A', 'P', 'L', 'B', 'S', 'D', 'P', 'C', 
    '/', 'p', 'p', 'c',
    '/', '\n', 'b', 'a', 'd', '1'
};
const uint8_t	bad_sysid2[] = {
    'A', 'A', 'P', 'L', 'B', 'S', 'D', 'P', 'C', 
    '/', 'p', 'p', 'c', 'n',
    '/', '\0', 'b', 'a', 'd', '2'
};
const uint8_t	bad_sysid3[] = {
    'A', 'A', 'P', 'L', 'B', 'S', 'D', 'P', 'C', 
    '/', 'p', '\0', 'c', 'a', 'b', 
    '/', 'b', 'a', 'd', '3'
};
const uint8_t	bad_sysid4[] = {
    'A', 'A', 'P', 'L', 'B', 'S', 'D', 'P', 'C', 
    '/', 'p', '\n', 'c',
    '/', 'b', 'a', 'd', 's', 'y', 's', '4'
};

#define BAD_SYSID_COUNT		4
struct {
    const uint8_t *	sysid;
    int			size;
} bad_sysids[BAD_SYSID_COUNT] = {
    { bad_sysid1, sizeof(bad_sysid1) },
    { bad_sysid2, sizeof(bad_sysid2) },
    { bad_sysid3, sizeof(bad_sysid3) },
    { bad_sysid4, sizeof(bad_sysid4) },
};
#endif TEST_BAD_SYSID
static const unsigned char	rfc_magic[4] = RFC_OPTIONS_MAGIC;

static const u_char dhcp_params[] = {
    dhcptag_vendor_class_identifier_e,
    dhcptag_vendor_specific_e,
};
#define N_DHCP_PARAMS	(sizeof(dhcp_params) / sizeof(dhcp_params[0]))

#define NetBoot2InfoVersion	0x33000
typedef enum {
    kNetBootVersionNone = 0,
    kNetBootVersion1 = 1,
    kNetBootVersion2 = 2
} NetBootVersion;

typedef enum {
    kBSDPClientStateInit = 0,
    kBSDPClientStateList = 1,
    kBSDPClientStateSelect = 2,
} BSDPClientState;

typedef void (*BSDPClientTimerCallBack)(BSDPClientRef client);

typedef union {
    BSDPClientListCallBack	list;
    BSDPClientSelectCallBack	select;
} BSDPClientCallBackUnion;

#define MAX_ATTRS	10

struct BSDPClient_s {
    char *				system_id;
    bsdp_version_t			client_version; /* network order */
    boolean_t				old_firmware;
    int					fd;
    u_short				client_port;
    CFSocketRef				socket;
    CFRunLoopSourceRef			rls;
    interface_t *			if_p;
    u_int32_t				xid;
    /*
     * send_buf is cast to some struct types containing short fields;
     * force it to be aligned as much as an int
     */
    int					send_buf[512];
    BSDPClientState			state;
    CFRunLoopTimerRef			timer;
    BSDPClientTimerCallBack		timer_callback;
    int					try;
    int					wait_secs;
    u_int16_t				attrs[MAX_ATTRS];
    int					n_attrs;
    struct in_addr			our_ip;
    boolean_t				got_responses;

    /* values provided by caller */
    struct {
	BSDPClientCallBackUnion		func;
	void *				arg;
	struct in_addr			server_ip;
	bsdp_image_id_t			image_identifier;
    } callback;

};

/* 
 * Function: mySCNetworkServicePathCopyServiceID
 * Purpose:
 *    Take a path of the form:
 *	"<domain>:/Network/Service/<serviceID>[/<entity>]";
 *    and return the <serviceID> portion of the string.
 */
static CFStringRef
mySCNetworkServicePathCopyServiceID(CFStringRef path)
{
    CFArrayRef		arr;
    CFStringRef		serviceID = NULL;

    arr = CFStringCreateArrayBySeparatingStrings(NULL, path, CFSTR("/"));
    if (arr == NULL) {
	goto done;
    }
    /* 
     * arr = {"<domain:>","Network","Service","<serviceID>"[,"<entity>"]} 
     * and we want the 4th component (arr[3]).
     */
    if (CFArrayGetCount(arr) < 4) {
	goto done;
    }
    serviceID = CFRetain(CFArrayGetValueAtIndex(arr, 3));
 done:
    if (arr != NULL) {
	CFRelease(arr);
    }
    return (serviceID);

}

static Boolean
get_dhcp_address(const char * ifname, struct in_addr * ret_ip)
{
    CFStringRef		ifname_cf = NULL;
    int			count;
    CFDictionaryRef	info_dict = NULL;
    int			i;
    const void * *	keys = NULL;
    CFStringRef		pattern;
    CFMutableArrayRef	patterns;
    Boolean		ret = FALSE;
    SCDynamicStoreRef	store;
    const void * *	values = NULL;

    store = SCDynamicStoreCreate(NULL, CFSTR("get_dhcp_address"),
				 NULL, NULL);
    if (store == NULL) {
	goto done;
    }
    ifname_cf = CFStringCreateWithCString(NULL, ifname, kCFStringEncodingASCII);

    /* pattern State:/Network/Service/[^/]+/DHCP */
    pattern 
	= SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainState,
						      kSCCompAnyRegex,
						      kSCEntNetDHCP);
    patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(patterns, pattern);
    CFRelease(pattern);
    pattern 
	= SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainState,
						      kSCCompAnyRegex,
						      kSCEntNetIPv4);
    CFArrayAppendValue(patterns, pattern);
    CFRelease(pattern);

    info_dict = SCDynamicStoreCopyMultiple(store, NULL, patterns);
    CFRelease(patterns);
    if (isA_CFDictionary(info_dict) == NULL) {
	goto done;
    }
    count = CFDictionaryGetCount(info_dict);
    values = malloc(sizeof(void *) * count);
    keys = malloc(sizeof(void *) * count);
    CFDictionaryGetKeysAndValues(info_dict, keys, values);
    for (i = 0; i < count; i++) {
	CFArrayRef	addrs;
	CFDictionaryRef	ipv4_dict;
	Boolean		got_match;
	CFStringRef	key;
	CFStringRef	name;
	CFStringRef	serviceID;

	if (CFStringHasSuffix(keys[i], kSCEntNetIPv4) == FALSE) {
	    continue;
	}
	ipv4_dict = isA_CFDictionary(values[i]);
	if (ipv4_dict == NULL) {
	    continue;
	}
	name = CFDictionaryGetValue(ipv4_dict, kSCPropInterfaceName);
	if (name == NULL) {
	    continue;
	}
	if (CFEqual(name, ifname_cf) == FALSE) {
	    continue;
	}
	/* look for the DHCP entity for this service ID */
	serviceID = mySCNetworkServicePathCopyServiceID(keys[i]);
	if (serviceID == NULL) {
	    continue;
	}
	key
	    = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  serviceID,
							  kSCEntNetDHCP);
	CFRelease(serviceID);
	got_match = CFDictionaryContainsKey(info_dict, key);
	CFRelease(key);
	if (got_match == FALSE) {
	    continue;
	}
	/* grab the IP address for this IPv4 dict */
	addrs = CFDictionaryGetValue(ipv4_dict, kSCPropNetIPv4Addresses);
	if (isA_CFArray(addrs) != NULL && CFArrayGetCount(addrs) > 0
	    && my_CFStringToIPAddress(CFArrayGetValueAtIndex(addrs, 0),
				      ret_ip)) {
	    ret = TRUE;
	}
	break;
    }
    
 done:
    if (ifname_cf != NULL) {
	CFRelease(ifname_cf);
    }
    if (values != NULL) {
	free(values);
    }
    if (keys != NULL) {
	free(keys);
    }
    if (info_dict != NULL) {
	CFRelease(info_dict);
    }
    if (store != NULL) {
	CFRelease(store);
    }
    return (ret);
}

static void
BSDPClientProcessList(BSDPClientRef client, struct in_addr server_ip,
		      struct dhcp * reply, int reply_len,
		      dhcpol_t * options_p, dhcpol_t * bsdp_options_p);
static void
BSDPClientProcessSelect(BSDPClientRef client, bsdp_msgtype_t bsdp_msg);

void
my_log(int priority, const char *message, ...)
{
    va_list 		ap;

    va_start(ap, message);
    vfprintf(stderr, message, ap);
    return;
}

static char * SystemIdentifierCopy(void);

#if defined(__ppc__) || defined(__ppc64__)
#define BSDP_ARCHITECTURE	"ppc"
static NetBootVersion
NetBootVersionGet()
{
    CFDictionaryRef		properties = NULL;
    CFDataRef			info = NULL;
    u_int32_t			version;
    NetBootVersion		support = kNetBootVersionNone;

    properties = myIORegistryEntryCopyValue("IODeviceTree:/rom/boot-rom");
    if (properties != NULL) {
	info = CFDictionaryGetValue(properties, CFSTR("info"));
    }
    if (info == NULL) {
	goto done;
    }
    CFDataGetBytes(info, CFRangeMake(8, sizeof(version)), (void *)&version);
    if (ntohl(version) < NetBoot2InfoVersion) {
	support = kNetBootVersion1;
    }
    else {
	support = kNetBootVersion2;
    }
 done:
    my_CFRelease(&properties);
    return (support);
}

static BSDPClientStatus
CopyNetBootVersionAndSystemIdentifier(NetBootVersion * version_p,
				      char * * system_id_p)
{
    *system_id_p = NULL;
    *version_p = NetBootVersionGet();
    if (*version_p == kNetBootVersionNone) {
	return (kBSDPClientStatusUnsupportedFirmware);
    }
    *system_id_p = SystemIdentifierCopy();
    if (*system_id_p == NULL) {
	return (kBSDPClientStatusAllocationError);
    }
    return (kBSDPClientStatusOK);
}

#elif defined(__i386__) || defined(__x86_64__)
#define BSDP_ARCHITECTURE	"i386"

static BSDPClientStatus
CopyNetBootVersionAndSystemIdentifier(NetBootVersion * version_p,
				      char * * system_id_p)
{
    *system_id_p = SystemIdentifierCopy();
    if (*system_id_p == NULL) {
	return (kBSDPClientStatusAllocationError);
    }
    *version_p = kNetBootVersion2;
    return (kBSDPClientStatusOK);
}

#else

static BSDPClientStatus
CopyNetBootVersionAndSystemIdentifier(NetBootVersion * version_p,
				      char * * system_id_p)
{
    return (kBSDPClientStatusUnsupportedFirmware);
}

#endif

static char *
SystemIdentifierCopy()
{
    CFDictionaryRef		properties = NULL;
    char *			system_id = NULL;
    CFDataRef			system_id_data = NULL;
    int				system_id_len = 0;

    properties = myIORegistryEntryCopyValue("IODeviceTree:/");
    if (properties != NULL) {
	system_id_data = CFDictionaryGetValue(properties, CFSTR("model"));
	if (system_id_data) {
	    system_id_len = CFDataGetLength(system_id_data);
	}
    }
    if (system_id_len == 0) {
	goto done;
    }
    system_id = (char *)malloc(system_id_len + 1);
    if (system_id == NULL) {
	goto done;
    }
    CFDataGetBytes(system_id_data, CFRangeMake(0, system_id_len),
		   (UInt8 *)system_id);
    system_id[system_id_len] = '\0';
    my_CFRelease(&properties);
    return (system_id);

 done:
    my_CFRelease(&properties);
    return (NULL);
}

#define RX_BUF_SIZE		(8 * 1024)

static struct dhcp * 
make_bsdp_request(char * system_id, struct dhcp * request, int pkt_size,
		  dhcp_msgtype_t msg, u_char * hwaddr, u_char hwtype, 
		  u_char hwlen, dhcpoa_t * options_p)
{
    char	vendor_class_id[DHCP_OPTION_SIZE_MAX];
    uint16_t	max_dhcp_message_size = htons(ETHERMTU);

    bzero(request, pkt_size);
    request->dp_op = BOOTREQUEST;
    request->dp_htype = hwtype;
    request->dp_hlen = hwlen;
    bcopy(hwaddr, request->dp_chaddr, hwlen);
    bcopy(rfc_magic, request->dp_options, sizeof(rfc_magic));
    dhcpoa_init(options_p, request->dp_options + sizeof(rfc_magic),
		pkt_size - sizeof(struct dhcp) - sizeof(rfc_magic));
    
    /* make the request a dhcp message */
    if (dhcpoa_add_dhcpmsg(options_p, msg) != dhcpoa_success_e) {
	fprintf(stderr,
	       "make_bsdp_request: couldn't add dhcp message tag %d, %s", msg,
	       dhcpoa_err(options_p));
	goto err;
    }

    /* add the list of required parameters */
    if (dhcpoa_add(options_p, dhcptag_parameter_request_list_e,
		   N_DHCP_PARAMS, dhcp_params)
	!= dhcpoa_success_e) {
	fprintf(stderr, "make_bsdp_request: "
	       "couldn't add parameter request list, %s",
	       dhcpoa_err(options_p));
	goto err;
    }
    /* add the max message size */
    if (dhcpoa_add(options_p, dhcptag_max_dhcp_message_size_e,
		   sizeof(max_dhcp_message_size), &max_dhcp_message_size)
	!= dhcpoa_success_e) {
	fprintf(stderr, "make_bsdp_request: "
	       "couldn't add max message size, %s",
	       dhcpoa_err(options_p));
	goto err;
    }
#ifndef TEST_BAD_SYSID
    /* add our vendor class identifier */
    snprintf(vendor_class_id, sizeof(vendor_class_id),
	     BSDP_VENDOR_CLASS_ID "/" BSDP_ARCHITECTURE "/%s", system_id);
    if (dhcpoa_add(options_p, 
		   dhcptag_vendor_class_identifier_e, 
		   strlen(vendor_class_id), vendor_class_id) 
	!= dhcpoa_success_e) {
	fprintf(stderr, "make_bsdp_request: add class id failed, %s",
		dhcpoa_err(options_p));
	goto err;
    }
#else TEST_BAD_SYSID
    { 
	static int	bad_sysid_index;

	if (dhcpoa_add(options_p,
		       dhcptag_vendor_class_identifier_e, 
		       bad_sysids[bad_sysid_index].size,
		       bad_sysids[bad_sysid_index].sysid)
	    != dhcpoa_success_e) {
	    fprintf(stderr, "make_bsdp_request: add class id failed, %s",
		    dhcpoa_err(options_p));
	    goto err;
	}
	bad_sysid_index++;
	if (bad_sysid_index == BAD_SYSID_COUNT) {
	    bad_sysid_index = 0;
	}
    }
#endif TEST_BAD_SYSID
    return (request);

  err:
    return (NULL);
}

static int
S_open_socket(u_short * ret_port)
{
    u_short			client_port;
    struct sockaddr_in 		me;
    socklen_t		   	me_len;
    int 			opt;
    int				sockfd;
    int 			status;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
	perror("socket");
	goto failed;
    }

    bzero((char *)&me, sizeof(me));
    me.sin_family = AF_INET;

    /* get a privileged port */
    opt = IP_PORTRANGE_LOW;
    status = setsockopt(sockfd, IPPROTO_IP, IP_PORTRANGE, &opt, 
			sizeof(opt));
    if (status < 0) {
	perror("setsockopt IPPROTO_IP IP_PORTRANGE");
	goto failed;
    }
    status = bind(sockfd, (struct sockaddr *)&me, sizeof(me));
    if (status != 0) {
	perror("bind");
	goto failed;
    }
    me_len = sizeof(me);
    if (getsockname(sockfd, (struct sockaddr *)&me,  &me_len) < 0) {
	perror("getsockname");
	goto failed;
    }
    client_port = ntohs(me.sin_port);
    opt = 1;
    status = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, 
			sizeof(opt));
    if (status < 0) {
	perror("setsockopt SO_BROADCAST");
	goto failed;
    }
    opt = 1;
    status = ioctl(sockfd, FIONBIO, &opt);
    if (status < 0) {
	perror("FIONBIO");
	goto failed;
    }
    *ret_port = client_port;
    return sockfd;

 failed:
    if (sockfd >= 0) {
	close(sockfd);
    }
    return (-1);
}

/* deprecated: BSDPImageDescriptionIndexIsServerLocal */
Boolean
BSDPImageDescriptionIndexIsServerLocal(CFNumberRef index)
{
    u_int16_t	index_val = 1;

    (void)CFNumberGetValue(index, kCFNumberShortType, &index_val);
    return (bsdp_image_index_is_server_local(index_val));
}

Boolean
BSDPImageDescriptionIdentifierIsServerLocal(CFNumberRef identifier)
{
    u_int32_t	identifier_val = 1;

    (void)CFNumberGetValue(identifier, kCFNumberSInt32Type, &identifier_val);
    return (bsdp_image_identifier_is_server_local(identifier_val));
}

Boolean
BSDPImageDescriptionIdentifierIsInstall(CFNumberRef identifier)
{
    u_int32_t	identifier_val = 1;

    (void)CFNumberGetValue(identifier, kCFNumberSInt32Type, &identifier_val);
    return (bsdp_image_identifier_is_install(identifier_val));
}

BSDPImageKind
BSDPImageDescriptionIdentifierImageKind(CFNumberRef identifier)
{
    u_int32_t	identifier_val = 1;

    (void)CFNumberGetValue(identifier, kCFNumberSInt32Type, &identifier_val);
    return (bsdp_image_kind_from_attributes(bsdp_image_attributes(identifier_val)));
}

/**
 ** BSDPClient timer functions
 **/

static void
BSDPClientProcessTimer(CFRunLoopTimerRef timer, void * info)
{
    BSDPClientRef	client;

    client = (BSDPClientRef)info;
    (*client->timer_callback)(client);
    return;
}

static void
BSDPClientCancelTimer(BSDPClientRef client)
{
    if (client->timer) {
	CFRunLoopTimerInvalidate(client->timer);
	my_CFRelease(&client->timer);
    }
    client->timer_callback = NULL;
    return;
}

static void
BSDPClientSetTimer(BSDPClientRef client, struct timeval rel_time,
		   BSDPClientTimerCallBack callback)
{
    CFRunLoopTimerContext 	context =  { 0, NULL, NULL, NULL, NULL };
    CFAbsoluteTime 		wakeup_time;

    BSDPClientCancelTimer(client);
    client->timer_callback = callback;
    wakeup_time = CFAbsoluteTimeGetCurrent() + rel_time.tv_sec 
	  + ((double)rel_time.tv_usec / USECS_PER_SEC);
    context.info = client;
    client->timer
	= CFRunLoopTimerCreate(NULL, wakeup_time,
			       0.0, 0, 0,
			       BSDPClientProcessTimer,
			       &context);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), client->timer,
		      kCFRunLoopDefaultMode);
    return;
}

/*
 * Function: BSDPClientProcess
 * Purpose:
 *   Process a packet received on our open socket.
 *   Ensure that the packet is a BSDP packet[DHCP ACK] that 
 *   matches our currently outstanding request.
 *
 *   Dispatch to the appropriate handler (list or select) 
 *   depending on our current running state.
 */
static void
BSDPClientProcess(CFSocketRef s, CFSocketCallBackType type, 
		  CFDataRef address, const void *data, void *info)
{
    dhcpol_t			bsdp_options;
    bsdp_msgtype_t		bsdp_msg;
    BSDPClientRef 		client = (BSDPClientRef)info;
    dhcpo_err_str_t		err;
    struct sockaddr_in 		from;
    socklen_t 			fromlen;
    int 			n;
    void *			opt;
    int				opt_len;
    dhcpol_t			options;
    uint32_t			receive_buf[RX_BUF_SIZE / sizeof(uint32_t)];
    struct dhcp *		reply;
    struct in_addr		server_ip;

    n = recvfrom(client->fd, receive_buf,
		 sizeof(receive_buf), 0,
		 (struct sockaddr *)&from, &fromlen);
    if (n < 0) {
	if (errno != EAGAIN) {
	    fprintf(stderr, "BSDPClientProcess: recvfrom %s", 
		    strerror(errno));
	}
	return;
    }
    if (n < sizeof(struct dhcp)) {
	/* packet is too short */
	return;
    }

    switch (client->state) {
    case kBSDPClientStateInit:
    default:
	/* throw it away */
	return;
    case kBSDPClientStateList:
    case kBSDPClientStateSelect:
	break;
    }

    reply = (struct dhcp *)receive_buf;
    if (dhcp_packet_match((struct bootp *)receive_buf, client->xid, 
			  (u_char) if_link_arptype(client->if_p),
			  if_link_address(client->if_p),
			  if_link_length(client->if_p)) == FALSE
	|| reply->dp_ciaddr.s_addr != client->our_ip.s_addr) {
	/* wasn't us */
	return;
    }

    dhcpol_init(&options);
    dhcpol_init(&bsdp_options);

    if (dhcpol_parse_packet(&options, reply, n, &err) == FALSE) {
	fprintf(stderr, 
		"BSDPClientProcess: dhcpol_parse_packet failed, %s\n",
		err.str);
	goto done;
    }

    /* get the DHCP message type */
    opt = dhcpol_find(&options, dhcptag_dhcp_message_type_e, NULL, NULL);
    if (opt == NULL || *((unsigned char *)opt) != dhcp_msgtype_ack_e) {
	goto done; /* response must be a DHCP ack */
    }

    /* get the vendor class identifier */
    opt = dhcpol_find(&options, dhcptag_vendor_class_identifier_e,
		      &opt_len, NULL);
    if (opt == NULL
	|| opt_len != strlen(BSDP_VENDOR_CLASS_ID)
	|| bcmp(opt, BSDP_VENDOR_CLASS_ID, opt_len)) {
	goto done; /* not BSDP */
    }
    /* get the server identifier */
    opt = dhcpol_find(&options, dhcptag_server_identifier_e,
		      &opt_len, NULL);
    if (opt == NULL || opt_len != sizeof(server_ip)) {
	goto done;
    }
    server_ip = *((struct in_addr *)opt);

    /* decode the BSDP options */
    if (dhcpol_parse_vendor(&bsdp_options, &options, &err) == FALSE) {
	fprintf(stderr, 
		"BSDPClientProcess: dhcpol_parse_vendor failed, %s", err.str);
	goto done;
    }
    /* get the BSDP message type */
    opt = dhcpol_find(&bsdp_options, bsdptag_message_type_e,
		      &opt_len, NULL);
    if (opt == NULL || opt_len != 1) {
	goto done; /* no message id */
    }
    bsdp_msg = *((unsigned char *)opt);
    switch (client->state) {
    case kBSDPClientStateInit:
    default:
	break;
    case kBSDPClientStateList:
	/* ACK[LIST] */
	if (bsdp_msg == bsdp_msgtype_list_e) {
	    BSDPClientProcessList(client, server_ip,
				  (struct dhcp *)receive_buf, n,
				  &options,
				  &bsdp_options);
	}
	break;
    case kBSDPClientStateSelect:
	/* ACK[SELECT] or ACK[FAILED] */
	if (bsdp_msg == bsdp_msgtype_select_e 
	    || bsdp_msg == bsdp_msgtype_failed_e) {
	    BSDPClientProcessSelect(client, bsdp_msg);
	}
	break;
    }
 done:
    dhcpol_free(&options);
    dhcpol_free(&bsdp_options);
    return;
}


/*
 * Function: BSDPClientCreateWithInterfaceAndAttributes
 * Purpose:
 *   Instantiate a BSDPClientRef, checking to ensure that the machine
 *   is NetBoot-compatible.
 */
BSDPClientRef
BSDPClientCreateWithInterfaceAndAttributes(BSDPClientStatus * status_p,
					   const char * ifname, 
					   const u_int16_t * attrs, int n_attrs)
{
    BSDPClientRef	client = NULL;
    u_short		client_port;
    bsdp_version_t	client_version = htons(BSDP_VERSION_1_1);
    CFSocketContext	context = { 0, NULL, NULL, NULL, NULL };
    interface_t *	if_p = NULL;
    interface_list_t *	ifl = NULL;
    int			fd = -1;
    boolean_t		old_firmware = FALSE;
    CFRunLoopSourceRef	rls = NULL;
    CFSocketRef		socket = NULL;
    BSDPClientStatus	status = kBSDPClientStatusAllocationError;
    char *		system_id = NULL;
    BSDPClientStatus	this_status;
    NetBootVersion	version;

    this_status = CopyNetBootVersionAndSystemIdentifier(&version, &system_id);
    if (this_status != kBSDPClientStatusOK) {
	status = this_status;
	goto cleanup;
    }
    if (version == kNetBootVersion1) {
	old_firmware = TRUE;
    }
    ifl = ifl_init();
    if (ifl == NULL) {
	goto cleanup;
    }

    if_p = ifl_find_name(ifl, ifname);
    if (if_p == NULL) {
	status = kBSDPClientStatusNoSuchInterface;
	goto cleanup;
    }
    if (if_ift_type(if_p) != IFT_ETHER) {
	status = kBSDPClientStatusInvalidArgument;
	goto cleanup;
    }
    /* make a persistent copy */
    if_p = if_dup(if_p);
    if (if_p == NULL) {
	goto cleanup;
    }
    if (if_inet_addr(if_p).s_addr == 0) {
	status = kBSDPClientStatusInterfaceNotConfigured;
	goto cleanup;
    }
    client = malloc(sizeof(*client));
    if (client == NULL) {
	goto cleanup;
    }
    bzero(client, sizeof(*client));

    /* use the DHCP-supplied address, if it is available */
    if (get_dhcp_address(ifname, &client->our_ip) == FALSE) {
	client->our_ip = if_inet_addr(if_p);
    }
    if (n_attrs > 0) {
	int	i;

	if (n_attrs > MAX_ATTRS) {
	    n_attrs = MAX_ATTRS;
	}
	for (i = 0; i < n_attrs; i++) {
	    client->attrs[i] = htons(attrs[i]);
	}
	client->n_attrs = n_attrs;
    }
    fd = S_open_socket(&client_port);
    if (fd < 0) {
	if (errno == EPERM || errno == EACCES) {
	    perror("socket");
	    status = kBSDPClientStatusPermissionDenied;
	}
	goto cleanup;
    }
    context.info = client;
    socket = CFSocketCreateWithNative(NULL, fd, kCFSocketReadCallBack,
				      BSDPClientProcess, &context);
    if (socket == NULL) {
	goto cleanup;
    }
    rls = CFSocketCreateRunLoopSource(NULL, socket, 0);
    if (rls == NULL) {
	goto cleanup;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, 
		       kCFRunLoopDefaultMode);
    client->system_id = system_id;
    client->client_version = client_version;
    client->old_firmware = old_firmware;
    client->fd = fd;
    client->rls = rls;
    client->client_port = client_port;
    client->socket = socket;
    client->xid = arc4random();
    client->if_p = if_p;
    client->state = kBSDPClientStateInit;
    ifl_free(&ifl);
    *status_p = kBSDPClientStatusOK;
    return (client);

 cleanup:
    if (rls != NULL) {
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(), rls, 
			      kCFRunLoopDefaultMode);
	/* release the run loop source */
	CFRelease(rls);
    }
    if (socket != NULL) {
	/* remove one socket reference, close the file descriptor */
	CFSocketInvalidate(socket);

	/* release the socket */
	CFRelease(socket);
	fd = -1;
    }
    if (fd >= 0) {
	close(fd);
    }
    if (client != NULL) {
	free(client);
    }
    if (ifl != NULL) {
	ifl_free(&ifl);
    }
    if (if_p != NULL) {
	if_free(&if_p);
    }
    if (system_id != NULL) {
	free(system_id);
    }
    *status_p = status;
    return (NULL);
}

/*
 * Function: BSDPClientCreateWithInterface
 * Purpose:
 *   Allocate a new session.
 */
BSDPClientRef
BSDPClientCreateWithInterface(BSDPClientStatus * status_p,
			      const char * ifname)
{
    return (BSDPClientCreateWithInterfaceAndAttributes(status_p, ifname, 
						       NULL, 0));
}

/*
 * Function: BSDPClientCreate
 * Purpose:
 *   Published entry point to instantiate a BSDPClientRef over "en0".
 *   XXX we should ask IOKit which interface is the primary.
 */
BSDPClientRef
BSDPClientCreate(BSDPClientStatus * status_p)
{
    return (BSDPClientCreateWithInterface(status_p, "en0"));
}

void
BSDPClientFree(BSDPClientRef * client_p)
{
    BSDPClientRef client;

    if (client_p == NULL) {
	return;
    }
    client = *client_p;
    if (client == NULL) {
	return;
    }
    BSDPClientCancelTimer(client);
    if (client->socket != NULL) {
	/* remove one socket reference, close the file descriptor */
	CFSocketInvalidate(client->socket);

	/* release the socket */
	CFRelease(client->socket);
    }
    if (client->rls != NULL) {
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(), client->rls, 
			      kCFRunLoopDefaultMode);
	/* release the run loop source */
	CFRelease(client->rls);
    }
    if (client->if_p != NULL) {
	if_free(&client->if_p);
    }
    if (client->system_id != NULL) {
	free(client->system_id);
    }
    free(client);
    *client_p = NULL;
    return;
}

/**
 ** BSDP List Routines
 **/
static boolean_t
attributes_match(u_int16_t attrs,
		 const u_int16_t * attrs_list, int n_attrs_list)
{
    int		i;

    if (attrs_list == NULL || n_attrs_list == 0) {
	return (TRUE);
    }
    for (i = 0; i < n_attrs_list; i++) {
	if (attrs_list[i] == attrs) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

static CFArrayRef
BSDPClientCreateImageList(BSDPClientRef client,
			  bsdp_image_id_t default_image_id,
			  bsdp_image_id_t selected_image_id,
			  void * image_list, int image_list_len)
{
    bsdp_image_description_t *	descr;
    CFMutableArrayRef		images = NULL;
    int				length;

    images = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    descr = image_list;
    for (length = image_list_len; length > sizeof(*descr); ) {
	u_int16_t		attributes;
	bsdp_image_id_t		boot_image_id;
	CFMutableDictionaryRef	this_dict = NULL;
	int			this_len;
	CFStringRef		cf_image_name = NULL;
	CFNumberRef		cf_image_id = NULL;
	CFNumberRef		cf_image_index = NULL;

	this_len = sizeof(*descr) + descr->name_length;
	if (length < this_len) {
	    fprintf(stderr, "short image list at offset %d\n",
		    (int)((void *)descr - image_list));
	    goto failed;
	}
	boot_image_id = ntohl(*((bsdp_image_id_t *)descr->boot_image_id));
	attributes = bsdp_image_attributes(boot_image_id);
	if (boot_image_id != BOOT_IMAGE_ID_NULL
	    && attributes_match(htons(attributes), 
				client->attrs, client->n_attrs)) {
	    uint32_t	index;

	    this_dict
		= CFDictionaryCreateMutable(NULL, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	    cf_image_id = CFNumberCreate(NULL, kCFNumberSInt32Type, 
					 &boot_image_id);
	    index = bsdp_image_index(boot_image_id);
	    cf_image_index = CFNumberCreate(NULL, kCFNumberSInt32Type, 
					    &index);
	    cf_image_name = CFStringCreateWithBytes(NULL, 
						    descr->name,
						    descr->name_length,
						    kCFStringEncodingUTF8,
						    TRUE);
	    if (this_dict != NULL && cf_image_id != NULL 
		&& cf_image_index != NULL && cf_image_name != NULL) {
		CFDictionarySetValue(this_dict, 
				     kBSDPImageDescriptionName, cf_image_name);
		CFDictionarySetValue(this_dict, 
				     kBSDPImageDescriptionIdentifier, 
				     cf_image_id);
		CFDictionarySetValue(this_dict, kBSDPImageDescriptionIndex,
				     cf_image_index);
		if (attributes & BSDP_IMAGE_ATTRIBUTES_INSTALL) {
		    CFDictionarySetValue(this_dict, 
					 kBSDPImageDescriptionIsInstall,
					 kCFBooleanTrue);
		}
		if (boot_image_id == default_image_id) {
		    CFDictionarySetValue(this_dict, 
					 kBSDPImageDescriptionIsDefault,
					 kCFBooleanTrue);
		}
		if (boot_image_id == selected_image_id) {
		    CFDictionarySetValue(this_dict, 
					 kBSDPImageDescriptionIsSelected,
					 kCFBooleanTrue);
		}
		CFArrayAppendValue(images, this_dict);
	    }
	    my_CFRelease(&cf_image_index);
	    my_CFRelease(&cf_image_id);
	    my_CFRelease(&cf_image_name);
	    my_CFRelease(&this_dict);
	}
	descr = ((void *)descr) + this_len;
	length -= this_len;
    }
    if (CFArrayGetCount(images) == 0) {
	goto failed;
    }
    return ((CFArrayRef)images);

 failed:
    my_CFRelease(&images);
    return (NULL);
}

static void
BSDPClientProcessList(BSDPClientRef client, struct in_addr server_ip,
		      struct dhcp * reply, int reply_len,
		      dhcpol_t * options_p, dhcpol_t * bsdp_options_p)
{
    CFNumberRef			cf_priority = NULL;
    CFStringRef			cf_server_ip = NULL;
    bsdp_image_id_t		default_image_id = BOOT_IMAGE_ID_NULL;
    void *			image_list = NULL;
    int				image_list_len = 0;
    void *			opt;
    int				opt_len;
    CFArrayRef			images = NULL;
    uint32_t			priority = 0;
    bsdp_image_id_t		selected_image_id = BOOT_IMAGE_ID_NULL;

    /* get the server priority */
    opt = dhcpol_find(bsdp_options_p, bsdptag_server_priority_e, &opt_len,
		      NULL);
    if (opt != NULL && opt_len == sizeof(bsdp_priority_t)) {
	priority = (uint32_t)ntohs(*((bsdp_priority_t *)opt));
    }
    /* get the default boot image */
    opt = dhcpol_find(bsdp_options_p, bsdptag_default_boot_image_e, &opt_len,
		      NULL);
    if (opt != NULL && opt_len == sizeof(default_image_id)) {
	default_image_id = ntohl(*((bsdp_image_id_t *)opt));
    }
    /* get the selected boot image */
    opt = dhcpol_find(bsdp_options_p, bsdptag_selected_boot_image_e, &opt_len,
		      NULL);
    if (opt && opt_len == sizeof(selected_image_id)) {
	selected_image_id = ntohl(*((bsdp_image_id_t *)opt));
    }
    /* get the list of images */
    image_list = dhcpol_option_copy(bsdp_options_p, bsdptag_boot_image_list_e,
				    &image_list_len);
    if (image_list == NULL) {
	goto done;
    }
    cf_priority = CFNumberCreate(NULL, kCFNumberSInt32Type, &priority);
    cf_server_ip = CFStringCreateWithCString(NULL, inet_ntoa(server_ip),
					     kCFStringEncodingASCII);
    images = BSDPClientCreateImageList(client, default_image_id, 
				       selected_image_id,
				       image_list, image_list_len);
    if (images != NULL && cf_priority != NULL && cf_server_ip != NULL) {
	client->got_responses = TRUE;
	(*client->callback.func.list)(client, 
				      kBSDPClientStatusOK,
				      cf_server_ip,
				      cf_priority,
				      images,
				      client->callback.arg);
    }

 done:
    my_CFRelease(&images);
    my_CFRelease(&cf_priority);
    my_CFRelease(&cf_server_ip);
    if (image_list != NULL) {
	free(image_list);
    }
    my_CFRelease(&images);
    return;
}

static BSDPClientStatus
BSDPClientSendListRequest(BSDPClientRef client)
{
    char		bsdp_buf[DHCP_OPTION_SIZE_MAX];
    dhcpoa_t		bsdp_options;
    char		buf[DHCP_PACKET_MIN];
    struct in_addr	ip_broadcast;
    dhcpoa_t		options;
    uint16_t		max_message_size = htons(RX_BUF_SIZE); /* max receive size */
    unsigned char	msgtype;
    u_int16_t		port = htons(client->client_port);
    struct dhcp *	request;
    int 		request_size = 0;
    BSDPClientStatus	status = kBSDPClientStatusAllocationError;

    ip_broadcast.s_addr = htonl(INADDR_BROADCAST);
    request = make_bsdp_request(client->system_id,
				(struct dhcp *)buf, sizeof(buf),
				dhcp_msgtype_inform_e, 
				if_link_address(client->if_p),
				if_link_arptype(client->if_p), 
				if_link_length(client->if_p), 
				&options);
    if (request == NULL) {
	goto failed;
    }
    request->dp_xid = htonl(client->xid);
    request->dp_ciaddr = client->our_ip;
    dhcpoa_init_no_end(&bsdp_options, bsdp_buf, sizeof(bsdp_buf));
    msgtype = bsdp_msgtype_list_e;
    if (dhcpoa_add(&bsdp_options, bsdptag_message_type_e,
		   sizeof(msgtype), &msgtype) 
	!= dhcpoa_success_e) {
	fprintf(stderr, "BSDPClientSendListRequest add message type failed, %s",
		dhcpoa_err(&bsdp_options));
	goto failed;
    }
    if (dhcpoa_add(&bsdp_options, bsdptag_version_e, 
		   sizeof(client->client_version),
		   &client->client_version) != dhcpoa_success_e) {
	fprintf(stderr, "BSDPClientSendListRequest add version failed, %s",
		dhcpoa_err(&bsdp_options));
	goto failed;
    }
    if (client->old_firmware == TRUE) {
	if (dhcpoa_add(&bsdp_options, bsdptag_netboot_1_0_firmware_e,
		       0, NULL) != dhcpoa_success_e) {
	    fprintf(stderr, "BSDPClientSendListRequest old_firmware failed, %s",
		    dhcpoa_err(&bsdp_options));
	    goto failed;
	}
    }
    if (dhcpoa_add(&bsdp_options, bsdptag_reply_port_e, sizeof(port), 
		   &port) != dhcpoa_success_e) {
	fprintf(stderr, "BSDPClientSendListRequest add reply port failed, %s",
		dhcpoa_err(&bsdp_options));
	goto failed;
    }
    if (dhcpoa_add(&bsdp_options, bsdptag_max_message_size_e,
		   sizeof(max_message_size), &max_message_size)
	!= dhcpoa_success_e) {
	fprintf(stderr,
		"BSDPClientSendListRequest add max message size failed, %s",
		dhcpoa_err(&bsdp_options));
	goto failed;
    }
    if (client->n_attrs > 0) {
	if (dhcpoa_add(&bsdp_options,bsdptag_image_attributes_filter_list_e,
		       client->n_attrs * sizeof(client->attrs[0]),
		       client->attrs) != dhcpoa_success_e) {
	    fprintf(stderr, 
		    "BSDPClientSendListRequest add image attributes failed, %s",
		    dhcpoa_err(&bsdp_options));
	    goto failed;
	}
    }
    if (dhcpoa_add(&options, dhcptag_vendor_specific_e,
		   dhcpoa_used(&bsdp_options), &bsdp_buf)
	!= dhcpoa_success_e) {
	fprintf(stderr, 
		"BSDPClientSendListRequest add vendor specific failed, %s",
	       dhcpoa_err(&options));
	goto failed;
    }
    if (dhcpoa_add(&options, dhcptag_end_e, 0, NULL)
	!= dhcpoa_success_e) {
	fprintf(stderr, 
		"BSDPClientSendListRequest add dhcp options end failed, %s",
		dhcpoa_err(&bsdp_options));
	goto failed;
    }
    request_size = sizeof(*request) + sizeof(rfc_magic) 
	+ dhcpoa_used(&options);
    if (request_size < sizeof(struct bootp)) {
	/* pad out to BOOTP-sized packet */
	request_size = sizeof(struct bootp);
    }
    if (bootp_transmit(client->fd, (char *)client->send_buf,
		       if_name(client->if_p), ARPHRD_ETHER, NULL, 0,
		       ip_broadcast, client->our_ip,
		       IPPORT_BOOTPS, client->client_port,
		       request, request_size) < 0) {
	fprintf(stderr,
		"BSDPClientSendListRequest: bootp_transmit failed %s\n",
		strerror(errno));
	status = kBSDPClientStatusTransmitFailed;
	goto failed;
    }
    status = kBSDPClientStatusOK;

 failed:
    return (status);
}

static void
BSDPClientListTimeout(BSDPClientRef client)
{
    BSDPClientStatus	status = kBSDPClientStatusOK;
    struct timeval	t;

    if (client->try == BSDPCLIENT_LIST_MAX_TRIES) {
	if (client->got_responses == FALSE) {
	    status = kBSDPClientStatusOperationTimedOut;
	    goto report_error;
	}
	return;
    }
    client->try++;
    client->wait_secs *= 2;
    if (client->wait_secs > BSDPCLIENT_MAX_WAIT_SECS) {
	client->wait_secs = BSDPCLIENT_MAX_WAIT_SECS;
    }
    status = BSDPClientSendListRequest(client);
    if (status != kBSDPClientStatusOK) {
	goto report_error;
    }
    t.tv_sec = client->wait_secs;
    t.tv_usec = random_range(0, USECS_PER_SEC - 1);
    BSDPClientSetTimer(client, t, BSDPClientListTimeout);
    return;

 report_error:
    (*client->callback.func.list)(client, status, NULL, NULL,
				  NULL, client->callback.arg);
    return;
}

BSDPClientStatus
BSDPClientList(BSDPClientRef client, BSDPClientListCallBack callback,
	       void * info)
{
    struct timeval	t;
    BSDPClientStatus	status = kBSDPClientStatusAllocationError;

    client->state = kBSDPClientStateInit;
    BSDPClientCancelTimer(client);
    if (callback == NULL) {
	status = kBSDPClientStatusInvalidArgument;
	goto failed;
    }
    client->xid++;
    status = BSDPClientSendListRequest(client);
    if (status != kBSDPClientStatusOK) {
	goto failed;
    }
    client->state = kBSDPClientStateList;
    client->try = 1;
    client->got_responses = FALSE;
    client->callback.func.list = callback;
    client->callback.arg = info;
    client->wait_secs = BSDPCLIENT_INITIAL_TIMEOUT_SECS;
    t.tv_sec = client->wait_secs;
    t.tv_usec = random_range(0, USECS_PER_SEC - 1);
    BSDPClientSetTimer(client, t, BSDPClientListTimeout);

 failed:
    return (status);
}

/**
 ** BSDP Select Routines
 **/

static void
BSDPClientProcessSelect(BSDPClientRef client, bsdp_msgtype_t bsdp_msg)
{
    BSDPClientStatus	status;

    BSDPClientCancelTimer(client);
    if (bsdp_msg == bsdp_msgtype_select_e) {
	status = kBSDPClientStatusOK;
    }
    else {
	status = kBSDPClientStatusServerSentFailure;
    }
    (*client->callback.func.select)(client, status, client->callback.arg);
    return;
}

static BSDPClientStatus
BSDPClientSendSelectRequest(BSDPClientRef client)
{
    char		bsdp_buf[DHCP_OPTION_SIZE_MAX];
    dhcpoa_t		bsdp_options;
    char		buf[DHCP_PACKET_MIN];
    bsdp_image_id_t	image_id = htonl(client->callback.image_identifier);
    struct in_addr	ip_broadcast;
    dhcpoa_t		options;
    unsigned char	msgtype;
    u_int16_t		port = htons(client->client_port);
    struct dhcp *	request;
    int 		request_size = 0;
    BSDPClientStatus	status = kBSDPClientStatusAllocationError;

    ip_broadcast.s_addr = htonl(INADDR_BROADCAST);
    request = make_bsdp_request(client->system_id,
				(struct dhcp *)buf, sizeof(buf),
				dhcp_msgtype_inform_e, 
				if_link_address(client->if_p),
				if_link_arptype(client->if_p), 
				if_link_length(client->if_p), 
				&options);
    if (request == NULL) {
	goto failed;
    }
    request->dp_xid = htonl(client->xid);
    request->dp_ciaddr = client->our_ip;
    dhcpoa_init_no_end(&bsdp_options, bsdp_buf, sizeof(bsdp_buf));
    msgtype = bsdp_msgtype_select_e;
    if (dhcpoa_add(&bsdp_options, bsdptag_message_type_e,
		   sizeof(msgtype), &msgtype) 
	!= dhcpoa_success_e) {
	fprintf(stderr, 
		"BSDPClientSendSelectRequest add message type failed, %s",
		dhcpoa_err(&bsdp_options));
	goto failed;
    }
    if (dhcpoa_add(&bsdp_options, bsdptag_version_e, 
		   sizeof(client->client_version),
		   &client->client_version) != dhcpoa_success_e) {
	fprintf(stderr, "BSDPClientSendSelectRequest add version failed, %s",
		dhcpoa_err(&bsdp_options));
	goto failed;
    }
    if (client->old_firmware == TRUE) {
	if (dhcpoa_add(&bsdp_options, bsdptag_netboot_1_0_firmware_e,
		       0, NULL) != dhcpoa_success_e) {
	    fprintf(stderr, "BSDPClientSendListRequest old_firmware failed, %s",
		    dhcpoa_err(&bsdp_options));
	    goto failed;
	}
    }
    if (dhcpoa_add(&bsdp_options, bsdptag_reply_port_e, sizeof(port), 
		   &port) != dhcpoa_success_e) {
	fprintf(stderr, "BSDPClientSendSelectRequest add reply port failed, %s",
		dhcpoa_err(&bsdp_options));
	goto failed;
    }
    if (dhcpoa_add(&bsdp_options, bsdptag_server_identifier_e,
		   sizeof(struct in_addr), 
		   &client->callback.server_ip) != dhcpoa_success_e) {
	fprintf(stderr, 
		"BSDPClientSendSelectRequest: add server identifier failed, %s",
		dhcpoa_err(&bsdp_options));
	goto failed;
    }
    if (dhcpoa_add(&bsdp_options, bsdptag_selected_boot_image_e,
		   sizeof(image_id), &image_id) != dhcpoa_success_e) {
	fprintf(stderr, 
		"BSDPClientSendSelectRequest: add selected image failed, %s",
		dhcpoa_err(&bsdp_options));
	goto failed;
    }
    if (dhcpoa_add(&options, dhcptag_vendor_specific_e,
		   dhcpoa_used(&bsdp_options), &bsdp_buf)
	!= dhcpoa_success_e) {
	fprintf(stderr, 
		"BSDPClientSendSelectRequest add vendor specific failed, %s",
	       dhcpoa_err(&options));
	goto failed;
    }
    if (dhcpoa_add(&options, dhcptag_end_e, 0, NULL)
	!= dhcpoa_success_e) {
	fprintf(stderr, 
		"BSDPClientSendSelectRequest add dhcp options end failed, %s",
		dhcpoa_err(&bsdp_options));
	goto failed;
    }
    request_size = sizeof(*request) + sizeof(rfc_magic) 
	+ dhcpoa_used(&options);
    if (request_size < sizeof(struct bootp)) {
	/* pad out to BOOTP-sized packet */
	request_size = sizeof(struct bootp);
    }
    /* send the packet */
    if (bootp_transmit(client->fd, (char *)client->send_buf,
		       if_name(client->if_p), ARPHRD_ETHER, NULL, 0,
		       ip_broadcast, client->our_ip,
		       IPPORT_BOOTPS, client->client_port,
		       request, request_size) < 0) {
	fprintf(stderr,
		"BSDPClientSendSelectRequest: bootp_transmit failed %s\n",
		strerror(errno));
	status = kBSDPClientStatusTransmitFailed;
	goto failed;
    }
    status = kBSDPClientStatusOK;

 failed:
    return (status);
}

static void
BSDPClientSelectTimeout(BSDPClientRef client)
{
    BSDPClientStatus	status = kBSDPClientStatusOK;
    struct timeval	t;

    if (client->try == BSDPCLIENT_SELECT_MAX_TRIES) {
	status = kBSDPClientStatusOperationTimedOut;
	goto report_error;
    }
    client->try++;
    client->wait_secs *= 2;
    status = BSDPClientSendSelectRequest(client);
    if (status != kBSDPClientStatusOK) {
	goto report_error;
    }
    t.tv_sec = client->wait_secs;
    t.tv_usec = 0;
    BSDPClientSetTimer(client, t, BSDPClientSelectTimeout);
    return;

 report_error:
    (*client->callback.func.select)(client, status, client->callback.arg);
    return;
}

BSDPClientStatus
BSDPClientSelect(BSDPClientRef client, 
		 CFStringRef ServerAddress,
		 CFNumberRef Identifier,
		 BSDPClientSelectCallBack callback, void * info)
{
    unsigned long	image_identifier;
    struct timeval	t;
    BSDPClientStatus	status = kBSDPClientStatusAllocationError;

    (void)my_CFStringToIPAddress(ServerAddress, &client->callback.server_ip);
    client->state = kBSDPClientStateInit;
    BSDPClientCancelTimer(client);
    if (callback == NULL
	|| CFNumberGetValue(Identifier, kCFNumberLongType, 
			    &image_identifier) == FALSE) {
	status = kBSDPClientStatusInvalidArgument;
	goto failed;
    }
    client->xid++;
    client->callback.image_identifier = image_identifier;
    status = BSDPClientSendSelectRequest(client);
    if (status != kBSDPClientStatusOK) {
	goto failed;
    }
    client->state = kBSDPClientStateSelect;
    client->try = 1;
    client->callback.func.select = callback;
    client->callback.arg = info;
    client->wait_secs = BSDPCLIENT_INITIAL_TIMEOUT_SECS;
    t.tv_sec = client->wait_secs;
    t.tv_usec = 0;
    BSDPClientSetTimer(client, t, BSDPClientSelectTimeout);

 failed:
    return (status);
}

BSDPClientStatus
BSPPClientSelect(BSDPClientRef client, 
		 CFStringRef ServerAddress,
		 CFNumberRef Identifier,
		 BSDPClientSelectCallBack callback, void * info)
{
    return (BSDPClientSelect(client, ServerAddress, Identifier,
			     callback, info));
}

#ifdef TEST_BAD_SYSID
static void bad_sysid_callback(BSDPClientRef client, 
			       BSDPClientStatus status,
			       CFStringRef ServerAddress,
			       CFNumberRef ServerPriority,
			       CFArrayRef list,
			       void *info)
{
    return;
}


int
main(int argc, char * argv[])
{
    BSDPClientStatus	status;
    char *		if_name = "en0";
    BSDPClientRef	client;

    if (argc > 1) {
	if_name = argv[1];
    }

    client = BSDPClientCreateWithInterface(&status, if_name);
    if (client == NULL) {
	fprintf(stderr, "BSDPClientCreateWithInterface(%s) failed: %s\n",
		if_name, BSDPClientStatusString(status));
	exit(2);
    }
    status = BSDPClientList(client, bad_sysid_callback, NULL);
    CFRunLoopRun();
    exit (0);
    return (0);
}
#endif TEST_BAD_SYSID
