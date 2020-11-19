/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
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
 * DHCPv6Server.c
 * - stateless DHCPv6 server
 */
/* 
 * Modification History
 *
 * August 28, 2018		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/filio.h>
#include <ctype.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/bootp.h>
#include <arpa/inet.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "DHCPDUID.h"
#include "DHCPv6.h"
#include "DHCPv6Server.h"
#include "DHCPv6Options.h"
#include "util.h"
#include <syslog.h>
#include <sys/uio.h>
#include <dispatch/dispatch.h>
#include "IPv6Socket.h"
#include "symbol_scope.h"
#include "IPv6Sock_Compat.h"
#include "cfutil.h"
#include "interfaces.h"


#define my_log		SC_log

/*
 * DUID File (/var/db/com.apple.dhcp6d.plist)
 * - a property list containing:
 *	"DUID"		<data>
 */
#define DHCP6D_DUID_FILE			\
    "/var/db/com.apple.dhcp6d.plist"
STATIC const char * kDHCPv6ServerDUIDFile = DHCP6D_DUID_FILE;
STATIC const CFStringRef kDHCPv6ServerDUID = CFSTR("DUID");


/*
 * Configuration File (com.apple.dhcp6d.plist)
 *
 * The DHCPv6 server configuration file uses the property list format with
 * a dictionary at the root.
 *
 * The following keys are defined in the top-level dictionary:
 *
 * "options" <dict>
 * - Each key in the dictionary corresponds to an option, e.g.
 *   "dhcp_dns_servers" and "dhcp_domain_list"
 * - The value of each key must be an appropriate type for the option.
 *   Both "dhcp_dns_servers" and "dhcp_domain_list" can have values that
 *   are either <string> or <array> of <string>.
 *
 * "enabled_interfaces"	<array> of <string>
 * - Each element of the array is a string corresponding to the interface name,
 *   e.g. "bridge100" or "en0".
 *
 * "verbose" <bool>
 * - boolean enables/disables verbose logging
 */
#define DHCP6D_CONFIG_FILE						\
    "/Library/Preferences/SystemConfiguration/com.apple.dhcp6d.plist"
STATIC const char * kDHCPv6ServerConfigurationFile = DHCP6D_CONFIG_FILE;
STATIC const CFStringRef kDHCPv6ServerEnabledInterfaces = CFSTR("enabled_interfaces");
STATIC const CFStringRef kDHCPv6ServerOptions = CFSTR("options");
STATIC const CFStringRef kDHCPv6ServerVerbose = CFSTR("verbose");

/*
 * Globals
 */
STATIC bool			S_verbose;
STATIC uint16_t			S_client_port = DHCPV6_CLIENT_PORT;
STATIC uint16_t			S_server_port = DHCPV6_SERVER_PORT;

STATIC const struct sockaddr_in6 dhcpv6_all_servers_and_relay_agents = {
    sizeof(dhcpv6_all_servers_and_relay_agents),
    AF_INET6, 0, 0,
    All_DHCP_Relay_Agents_and_Servers_INIT, 0
};

STATIC const struct sockaddr_in6 blank_sin6 = {
    sizeof(blank_sin6),
    AF_INET6, 0, 0,
    IN6ADDR_ANY_INIT, 0
};

typedef unsigned int	IFIndex;

struct DHCPv6Server {
    int			sock_fd;
    dispatch_source_t	sock_source;

    CFDataRef		duid;
    char *		config_file;

    /* global options */
    CFDictionaryRef	options;

    /* keep a copy so that we can re-evaluate when the interface list changes */
    CFArrayRef		enabled_interfaces;
    SCDynamicStoreRef	store;

    /*
     * if_indices, if_names are parallel arrays of size if_count
     */
    IFIndex *		if_indices;
    char * *		if_names;
    int			if_count;
};

STATIC void
DHCPv6ServerSetEnabledInterfaces(DHCPv6ServerRef server,
				 CFArrayRef enabled_interfaces);
STATIC int
DHCPv6ServerTransmit(DHCPv6ServerRef server,
		     IFIndex if_index,
		     const struct in6_addr * dst_p,
		     DHCPv6PacketRef pkt, int pkt_len);

STATIC int
set_multicast_for_interface(int sock_fd, int sopt, IFIndex if_index)
{
    struct group_req	mcr;
    int			ret;

    switch (sopt) {
    case MCAST_JOIN_GROUP:
    case MCAST_LEAVE_GROUP:
	break;
    default:
	return (-1);
    }
    mcr.gr_interface = if_index;
    memcpy(&mcr.gr_group, &dhcpv6_all_servers_and_relay_agents,
	   sizeof(dhcpv6_all_servers_and_relay_agents));
    ret = setsockopt(sock_fd, IPPROTO_IPV6, sopt, &mcr, sizeof(mcr));
    if (ret != 0) {
	my_log(LOG_ERR, "setsockopt(%s) if_index %d failed, %s",
	       (sopt == MCAST_JOIN_GROUP) ? "MCAST_JOIN_GROUP"
	       : "MCAST_LEAVE_GROUP",
	       if_index,
	       strerror(errno));
    }
    return (ret);
}

STATIC int
open_dhcpv6_socket(uint16_t port)
{
    struct sockaddr_in6		me;
    int				opt = 1;
    int 			sock_fd;

    sock_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
	my_log(LOG_ERR, "socket failed, %s", strerror(errno));
	return (sock_fd);
    }
    bzero(&me, sizeof(me));
    me.sin6_family = AF_INET6;
    me.sin6_port = htons(port);
    if (bind(sock_fd, (struct sockaddr *)&me, sizeof(me)) != 0) {
	my_log(LOG_ERR, "bind failed, %s",
	       strerror(errno));
	goto failed;
    }
    /* set non-blocking I/O */
    if (ioctl(sock_fd, FIONBIO, &opt) < 0) {
	my_log(LOG_ERR, "ioctl FIONBIO failed, %s",
	       strerror(errno));
	goto failed;
    }
    /* ask for packet info */
    if (setsockopt(sock_fd, IPPROTO_IPV6, 
		   IPCONFIG_SOCKOPT_PKTINFO, &opt, sizeof(opt)) < 0) {
	my_log(LOG_ERR, "setsockopt(IPV6_PKTINFO) failed, %s",
	       strerror(errno));
	goto failed;
    }

#if defined(SO_RECV_ANYIF)
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RECV_ANYIF, (caddr_t)&opt,
		   sizeof(opt)) < 0) {
	my_log(LOG_ERR, "setsockopt(SO_RECV_ANYIF) failed, %s",
	       strerror(errno));
    }
#endif /* SO_RECV_ANYIF */

#if defined(SO_TRAFFIC_CLASS)
    opt = SO_TC_CTL;
    /* set traffic class */
    if (setsockopt(sock_fd, SOL_SOCKET, SO_TRAFFIC_CLASS, &opt,
		   sizeof(opt)) < 0) {
	my_log(LOG_ERR, "setsockopt(SO_TRAFFIC_CLASS) failed, %s",
	       strerror(errno));
    }
#endif /* SO_TRAFFIC_CLASS */
    return (sock_fd);

 failed:
    close(sock_fd);
    return (-1);
}

#define kIFIndexListItemNotFound	(-1)

STATIC int
IFIndexListGetItemIndex(const IFIndex * list, int list_count, CFIndex if_index)
{
    for (int i = 0; i < list_count; i++) {
	if (list[i] == if_index) {
	    return (i);
	}
    }
    return (kIFIndexListItemNotFound);
}

STATIC Boolean
IFIndexListContainsItem(const IFIndex * list, int list_count, CFIndex if_index)
{
    return (IFIndexListGetItemIndex(list, list_count, if_index)
	    != kIFIndexListItemNotFound);
}

STATIC Boolean
DHCPv6ServerInterfaceIsEnabled(DHCPv6ServerRef server, IFIndex if_index)
{
    return (IFIndexListContainsItem(server->if_indices,
				    server->if_count, if_index));
}

STATIC const char *
DHCPv6ServerGetEnabledInterfaceName(DHCPv6ServerRef server, IFIndex if_index)
{
    int		which;

    which = IFIndexListGetItemIndex(server->if_indices, server->if_count,
				    if_index);
    if (which == kIFIndexListItemNotFound) {
	return (NULL);
    }
    return (server->if_names[which]);
}

STATIC void
DHCPv6ServerProcessRequest(DHCPv6ServerRef server,
			   const struct sockaddr_in6 * from_p,
			   DHCPv6PacketRef pkt, int pkt_len,
			   IFIndex if_index)
{
    char			buf[1500];
    DHCPDUIDRef			client_id;
    int				error;
    const char *		if_name;
    DHCPv6OptionErrorString 	err;
    char 			ntopbuf[INET6_ADDRSTRLEN];
    DHCPv6OptionListRef		options;
    int				option_len;
    DHCPv6OptionArea		oa;
    DHCPv6PacketRef		reply_pkt;
    const void *		requested_options;
    DHCPDUIDRef			server_id;

    if_name = DHCPv6ServerGetEnabledInterfaceName(server, if_index);
    if (if_name == NULL) {
	my_log(LOG_DEBUG, "Interface %d not enabled, ignoring (%d bytes)",
	       if_index, pkt_len);
	return;
    }
    if (pkt->msg_type != kDHCPv6MessageINFORMATION_REQUEST) {
	/* we only handle stateless requests */
	my_log(LOG_DEBUG, "Ignoring %s (%d) packet on interface %d (%d bytes)",
	       DHCPv6MessageName(pkt->msg_type), pkt->msg_type,
	       if_index, pkt_len);
	return;
    }
    options = DHCPv6OptionListCreateWithPacket(pkt, pkt_len, &err);
    if (S_verbose) {
	CFMutableStringRef	str;

	str = CFStringCreateMutable(NULL, 0);
	DHCPv6PacketPrintToString(str, pkt, pkt_len);
	if (options != NULL) {
	    DHCPv6OptionListPrintToString(str, options);
	}
	my_log(~LOG_NOTICE, "[%s] Receive from %s %@",
	       if_name,
	       inet_ntop(AF_INET6,
			 &from_p->sin6_addr, ntopbuf, sizeof(ntopbuf)),
	       str);
	CFRelease(str);
    }
    else {
	my_log(LOG_NOTICE, "[%s] Receive %s (%d) [%d bytes] from %s",
	       if_name, 
	       DHCPv6MessageName(pkt->msg_type), pkt->msg_type, pkt_len,
	       inet_ntop(AF_INET6,
			 &from_p->sin6_addr, ntopbuf, sizeof(ntopbuf)));
    }
    if (options == NULL) {
	my_log(LOG_NOTICE, "DHCPv6 options parse failed, %s",
	       err.str);
	return;
    }
    /* if ServerIdentifier present, it must match our DUID */
    server_id = (DHCPDUIDRef)
	DHCPv6OptionListGetOptionDataAndLength(options,
					       kDHCPv6OPTION_SERVERID,
					       &option_len, NULL);
    if (server_id != NULL) {
	if (!DHCPDUIDIsValid(server_id, option_len)) {
	    my_log(LOG_NOTICE, "Request contains invalid SERVERID");
	    goto done;
	}
	if (CFDataGetLength(server->duid) != option_len
	    || bcmp(server_id,
		    CFDataGetBytePtr(server->duid), option_len) != 0) {
	    my_log(LOG_NOTICE, "Request SERVERID doesn't match");
	    goto done;
	}
    }

    /* copy transaction-id and ClientIdentifier (if present) to REPLY */
    reply_pkt = (DHCPv6PacketRef)buf;
    DHCPv6PacketSetMessageType(reply_pkt, kDHCPv6MessageREPLY);
    memcpy(reply_pkt->transaction_id, pkt->transaction_id,
	   sizeof(reply_pkt->transaction_id));
    DHCPv6OptionAreaInit(&oa, reply_pkt->options, 
			 sizeof(buf) - DHCPV6_PACKET_HEADER_LENGTH);
    client_id = (DHCPDUIDRef)
	DHCPv6OptionListGetOptionDataAndLength(options,
					       kDHCPv6OPTION_CLIENTID,
					       &option_len, NULL);
    if (client_id != NULL) {
	if (!DHCPDUIDIsValid(client_id, option_len)) {
	    my_log(LOG_NOTICE, "Request contains invalid CLIENTID");
	    goto done;
	}
	if (!DHCPv6OptionAreaAddOption(&oa, kDHCPv6OPTION_CLIENTID,
				       option_len, client_id, &err)) {
	    my_log(LOG_NOTICE, " failed to add CLIENTID, %s",
		   err.str);
	    goto done;
	}
    }

    /* add our ServerIdentifier */
    if (!DHCPv6OptionAreaAddOption(&oa, kDHCPv6OPTION_SERVERID,
				   CFDataGetLength(server->duid),
				   CFDataGetBytePtr(server->duid),
				   &err)) {
	my_log(LOG_NOTICE, "failed to add SERVERID, %s",
	       err.str);
	goto done;
    }

    /* process OptionRequest option, and add our configuration for each */
    requested_options =
	DHCPv6OptionListGetOptionDataAndLength(options,
					       kDHCPv6OPTION_ORO,
					       &option_len, NULL);
    if (requested_options != NULL && server->options != NULL) {
	const void *	scan;

	scan = requested_options;
	for (int i = 0; i < (option_len / sizeof(DHCPv6OptionLength));
	     i++, scan += sizeof(DHCPv6OptionLength)) {
	    CFDataRef		data;
	    DHCPv6OptionCode	code;

	    code = net_uint16_get(scan);
	    data = DHCPv6OptionsDictionaryGetOption(server->options, code);
	    if (data != NULL) {
		if (!DHCPv6OptionAreaAddOption(&oa, code,
					       CFDataGetLength(data),
					       CFDataGetBytePtr(data),
					       &err)) {
		    my_log(LOG_NOTICE, "failed to add %s, %s",
			   DHCPv6OptionCodeGetName(code),
			   err.str);
		    goto done;
		}
	    }
	}
    }

    /* send a reply */
    error = DHCPv6ServerTransmit(server, if_index, &from_p->sin6_addr,
				 reply_pkt,
				 DHCPV6_PACKET_HEADER_LENGTH
				 + DHCPv6OptionAreaGetUsedLength(&oa));
    switch (error) {
    case 0:
    case ENXIO:
    case ENETDOWN:
	break;
    default:
	my_log(LOG_NOTICE, "%s transmit failed, %s", if_name, strerror(error));
	break;
    }


 done:
    DHCPv6OptionListRelease(&options);
    return;
}

STATIC void
DHCPv6ServerReceive(DHCPv6ServerRef server)
{
    struct cmsghdr *	cm;
    char		cmsgbuf[CMSG_SPACE(sizeof(struct in6_pktinfo))];
    struct sockaddr_in6 from;
    struct iovec 	iov;
    struct msghdr 	mhdr;
    ssize_t		n;
    struct in6_pktinfo *pktinfo = NULL;
    char	 	receive_buf[1500];

    /* initialize msghdr for receiving packets */
    iov.iov_base = (caddr_t)receive_buf;
    iov.iov_len = sizeof(receive_buf);
    mhdr.msg_name = (caddr_t)&from;
    mhdr.msg_namelen = sizeof(from);
    mhdr.msg_iov = &iov;
    mhdr.msg_iovlen = 1;
    mhdr.msg_control = (caddr_t)cmsgbuf;
    mhdr.msg_controllen = sizeof(cmsgbuf);

    /* get message */
    n = recvmsg(server->sock_fd, &mhdr, 0);
    if (n < 0) {
	if (errno != EAGAIN) {
	    my_log(LOG_ERR, "DHCPv6SocketRead: recvfrom failed %s (%d)",
		   strerror(errno), errno);
	}
	return;
    }
    if (n < DHCPV6_PACKET_HEADER_LENGTH) {
	my_log(LOG_NOTICE,
	       "DHCPv6SocketRead: packet too short %ld < %d",
	       n, DHCPV6_PACKET_HEADER_LENGTH);
	return;
    }

    /* get the control message that has the interface index */
    pktinfo = NULL;
    for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(&mhdr);
	 cm != NULL;
	 cm = (struct cmsghdr *)CMSG_NXTHDR(&mhdr, cm)) {
	if (cm->cmsg_level != IPPROTO_IPV6) {
	    continue;
	}
	switch (cm->cmsg_type) {
	case IPV6_PKTINFO:
	    if (cm->cmsg_len < CMSG_LEN(sizeof(struct in6_pktinfo))) {
		continue;
	    }
	    /* ALIGN: CMSG_DATA is should return aligned data */ 
	    pktinfo = (struct in6_pktinfo *)(void *)(CMSG_DATA(cm));
	    break;
	default:
	    /* this should never occur */
	    my_log(LOG_NOTICE, "Why did we get control message type %d?",
		   cm->cmsg_type);
	    break;
	}
    }
    if (pktinfo == NULL) {
	my_log(LOG_NOTICE,
	       "DHCPv6SocketRead: missing IPV6_PKTINFO");
	return;
    }
    DHCPv6ServerProcessRequest(server, &from,
			       (DHCPv6PacketRef)receive_buf, (int)n,
			       pktinfo->ipi6_ifindex);
    return;
}

STATIC void
WriteServerDUID(CFDataRef duid)
{
    CFDictionaryRef		dict;

    dict = CFDictionaryCreate(NULL, 
			      (const void * *)&kDHCPv6ServerDUID,
			      (const void * *)&duid,
			      1,
			      &kCFTypeDictionaryKeyCallBacks,
			      &kCFTypeDictionaryValueCallBacks);
    if (my_CFPropertyListWriteFile(dict, kDHCPv6ServerDUIDFile, 0644) < 0) {
	my_log(LOG_DEBUG, "Failed to write DUID to %s", kDHCPv6ServerDUIDFile);
    }
    my_CFRelease(&dict);
    return;
}

STATIC CFDataRef
CreateServerDUID(void)
{
    CFDataRef		duid = NULL;
    interface_t *	if_p;
    interface_list_t *	interfaces;

    interfaces = ifl_init();
    if (interfaces == NULL) {
	my_log(LOG_NOTICE, "can't retrieve interface list");
	goto done;
    }
    if_p = ifl_find_stable_interface(interfaces);
    if (if_p == NULL) {
	my_log(LOG_NOTICE, "can't find suitable interface for DUID");
	goto done;
    }
    duid = DHCPDUID_LLTDataCreate(if_link_address(if_p),
				  if_link_length(if_p),
				  if_link_arptype(if_p));
    if (duid == NULL) {
	my_log(LOG_NOTICE, "failed to establish DUID");
	goto done;
    }
    WriteServerDUID(duid);
    my_log(LOG_NOTICE, "Derived DUID from %s", if_name(if_p));

 done:
    ifl_free(&interfaces);
    return (duid);
}

STATIC CFDataRef
CopyServerDUID(void)
{
    CFDictionaryRef	dict;
    CFDataRef		duid = NULL;

    dict = my_CFPropertyListCreateFromFile(kDHCPv6ServerDUIDFile);
    if (isA_CFDictionary(dict) != NULL) {
	duid = CFDictionaryGetValue(dict, kDHCPv6ServerDUID);
	duid = isA_CFData(duid);
    }
    if (duid != NULL) {
	CFRetain(duid);
    }
    else {
	duid = CreateServerDUID();
    }
    if (duid != NULL) {
	CFMutableStringRef	str;

	str = CFStringCreateMutable(NULL, 0);
	DHCPDUIDPrintToString(str, (const DHCPDUIDRef)
			      CFDataGetBytePtr(duid),
			      (int)CFDataGetLength(duid));

	my_log(LOG_NOTICE, "%@", str);
	CFRelease(str);
    }
    my_CFRelease(&dict);
    return (duid);
}

STATIC void
close_socket(void * ctx)
{
    int	sock_fd = (int)(uintptr_t)ctx;

    close(sock_fd);
    return;
}

STATIC void
DHCPv6ServerHandleNotification(SCDynamicStoreRef session,
			       CFArrayRef changes,
			       void * info)
{
    DHCPv6ServerRef	server = (DHCPv6ServerRef)info;

    DHCPv6ServerSetEnabledInterfaces(server,
				     server->enabled_interfaces);
    return;
}

STATIC void
DHCPv6ServerEnableNotifications(DHCPv6ServerRef server)
{
    SCDynamicStoreContext	context;
    CFStringRef			key;
    CFArrayRef			patterns;
    SCDynamicStoreRef		store;

    bzero(&context, sizeof(context));
    context.info = server;
    store = SCDynamicStoreCreate(NULL, CFSTR("DHCPv6Server"),
				 DHCPv6ServerHandleNotification, &context);
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							kSCCompAnyRegex,
							kSCEntNetIPv6);
    patterns = CFArrayCreate(NULL, (const void **)&key, 1,
			     &kCFTypeArrayCallBacks);
    CFRelease(key);
    SCDynamicStoreSetNotificationKeys(store, NULL, patterns);
    CFRelease(patterns);
    server->store = store;
    SCDynamicStoreSetDispatchQueue(store, dispatch_get_main_queue());
    return;
}

PRIVATE_EXTERN DHCPv6ServerRef
DHCPv6ServerCreate(const char * config_file)
{
    CFDataRef		duid = NULL;
    DHCPv6ServerRef	server;
    int			sock_fd = -1;

    sock_fd = open_dhcpv6_socket(S_server_port);
    if (sock_fd < 0) {
	my_log(LOG_NOTICE, "socket() failed, %s", strerror(errno));
	goto failed;
    }
    duid = CopyServerDUID();
    if (duid == NULL) {
	my_log(LOG_NOTICE, "Can't load DUID");
	goto failed;
    }

    /* allocate the DHCPv6Server instance */
    server = malloc(sizeof(*server));
    bzero(server, sizeof(*server));
    if (config_file != NULL) {
	server->config_file = strdup(config_file);
    }
    else {
	server->config_file = strdup(kDHCPv6ServerConfigurationFile);
    }
    server->duid = duid;
    server->sock_fd = sock_fd;
    server->sock_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ,
						 sock_fd,
						 0UL,
						 dispatch_get_main_queue());
    dispatch_set_context(server->sock_source, (void *)(uintptr_t)sock_fd);
    dispatch_set_finalizer_f(server->sock_source, close_socket);
    dispatch_source_set_event_handler(server->sock_source,
				      ^{ DHCPv6ServerReceive(server); });
    DHCPv6ServerUpdateConfiguration(server);
    DHCPv6ServerEnableNotifications(server);
    dispatch_resume(server->sock_source);
    return (server);

 failed:
    if (sock_fd >= 0) {
	close(sock_fd);
    }
    my_CFRelease(&duid);
    return (NULL);
}

STATIC int
my_if_nametoindex(struct if_nameindex * name_index, const char * if_name)
{
    if (name_index == NULL) {
	return (0);
    }
    for (struct if_nameindex * scan = name_index;
	 scan->if_name != NULL;
	 scan++) {
	if (strcmp(scan->if_name, if_name) == 0) {
	    return (scan->if_index);
	}
    }
    return (0);
}

STATIC IFIndex *
create_if_indices(char * * if_names, int if_count)
{
    IFIndex *			if_indices;
    struct if_nameindex *	name_index = NULL;

    name_index = if_nameindex();

    if_indices = malloc(sizeof(*if_indices) * if_count);

    /* find the corresponding interface indices */
    for (int i = 0; i < if_count; i++) {
	if_indices[i] = my_if_nametoindex(name_index, if_names[i]);
    }
    if (name_index != NULL) {
	if_freenameindex(name_index);
    }
    return (if_indices);
}

STATIC void
DHCPv6ServerEnableMulticastForAddedInterfaces(DHCPv6ServerRef server,
					      IFIndex * if_indices,
					      char * * if_names,
					      int if_count)
{
    /* for each new item that is not in the old list, add */
    for (int i = 0; i < if_count; i++) {
	int	if_index = if_indices[i];
	char *	if_name = if_names[i]; 

	if (if_index != 0
	    && !DHCPv6ServerInterfaceIsEnabled(server, if_index)) {
	    if (set_multicast_for_interface(server->sock_fd, MCAST_JOIN_GROUP,
					    if_index) == 0) {
		my_log(LOG_NOTICE,
		       "Added DHCPv6 multicast for interface %s", if_name);
	    }
	}
    }
    return;
}

STATIC void
DHCPv6ServerDisableMulticastForRemovedInterfaces(DHCPv6ServerRef server,
						 IFIndex * if_indices,
						 int if_count)
{
    /* for each old item that is not in the new list, remove */
    for (int i = 0; i < server->if_count; i++) {
	int		if_index  = server->if_indices[i];
	const char *	if_name = server->if_names[i];

	if (if_index == 0) {
	    continue;
	}
	if (if_indices == NULL
	    || !IFIndexListContainsItem(if_indices, if_count, if_index)) {
	    if (set_multicast_for_interface(server->sock_fd, MCAST_LEAVE_GROUP,
					    if_index) == 0) {
		my_log(LOG_NOTICE,
		       "Removed DHCPv6 multicast for interface %s", if_name);
	    }
	}
    }
    return;
}

STATIC void
DHCPv6ServerSetEnabledInterfaces(DHCPv6ServerRef server,
				 CFArrayRef enabled_interfaces)
{
    int			if_count = 0;
    IFIndex *		if_indices = NULL;
    char * *		if_names = NULL;
    CFArrayRef		old_enabled_interfaces;

    old_enabled_interfaces = server->enabled_interfaces;
    if (S_verbose
	&& !my_CFEqual(old_enabled_interfaces, enabled_interfaces)) {
	if (enabled_interfaces != NULL) {
	    my_log(LOG_NOTICE, "Enabled Interfaces: %@", enabled_interfaces);
	}
	else {
	    my_log(LOG_NOTICE, "Enabled Interfaces: none");
	}
    }
    if (enabled_interfaces != NULL) {
	if_names = my_CStringArrayCreate(enabled_interfaces, &if_count);
	if (if_names != NULL) {
	    if_indices = create_if_indices(if_names, if_count);
	    if (if_indices != NULL) {
		DHCPv6ServerEnableMulticastForAddedInterfaces(server,
							      if_indices,
							      if_names,
							      if_count);
	    }
	}
    }
    DHCPv6ServerDisableMulticastForRemovedInterfaces(server, if_indices,
						     if_count);

    /* replace our interface lists (indices and names) */
    if (server->if_indices != NULL) {
	free(server->if_indices);
    }
    if (server->if_names != NULL) {
	free(server->if_names);
    }
    server->if_count = if_count;
    server->if_indices = if_indices;
    server->if_names = if_names;

    /* remember the previous value */
    if (enabled_interfaces != NULL) {
	CFRetain(enabled_interfaces);
    }
    server->enabled_interfaces = enabled_interfaces;
    my_CFRelease(&old_enabled_interfaces);
    return;
}

STATIC void
DHCPv6ServerSetGlobalOptions(DHCPv6ServerRef server, CFDictionaryRef options)
{
    CFDictionaryRef	new_options = NULL;

    if (options != NULL) {
	new_options = DHCPv6OptionsDictionaryCreate(options);
	if (new_options == NULL) {
	    my_log(LOG_NOTICE, "Failed to create DHCPv6OptionsDictionary");
	}
    }
    my_CFRelease(&server->options);
    server->options = new_options;
}

STATIC void
DHCPv6ServerSetConfiguration(DHCPv6ServerRef server, CFDictionaryRef plist)
{
    CFArrayRef		enabled_interfaces = NULL;
    CFDictionaryRef	options = NULL;

    if (plist != NULL) {
	CFBooleanRef	verbose;

	enabled_interfaces
	    =  CFDictionaryGetValue(plist, kDHCPv6ServerEnabledInterfaces);
	enabled_interfaces = isA_CFArray(enabled_interfaces);
	options = CFDictionaryGetValue(plist, kDHCPv6ServerOptions);
	options = isA_CFDictionary(options);
	verbose = CFDictionaryGetValue(plist, kDHCPv6ServerVerbose);
	if (isA_CFBoolean(verbose) != NULL) {
	    Boolean	new_verbose;

	    new_verbose = CFBooleanGetValue(verbose);
	    if (new_verbose != S_verbose) {
		S_verbose = new_verbose;
		my_log(LOG_NOTICE, "Verbose mode %s",
		       S_verbose ? "enabled" : "disabled");
	    }
	}
    }
    DHCPv6ServerSetEnabledInterfaces(server, enabled_interfaces);
    DHCPv6ServerSetGlobalOptions(server, options);
    return;
}

PRIVATE_EXTERN void
DHCPv6ServerUpdateConfiguration(DHCPv6ServerRef server)
{
    CFDictionaryRef	dict;

    dict = my_CFPropertyListCreateFromFile(server->config_file);
    if (isA_CFDictionary(dict) == NULL) {
	my_log(LOG_NOTICE, "Failed to load '%s'",
	       server->config_file);
    }
    DHCPv6ServerSetConfiguration(server, isA_CFDictionary(dict));
    my_CFRelease(&dict);
    return;
}

PRIVATE_EXTERN void
DHCPv6ServerRelease(DHCPv6ServerRef * server_p)
{
    DHCPv6ServerRef 	server = *server_p;

    if (server == NULL) {
	return;
    }
    if (server->sock_source != NULL) {
	dispatch_source_cancel(server->sock_source);
	dispatch_release(server->sock_source);
	server->sock_source = NULL;
    }
    if (server->config_file != NULL) {
	free(server->config_file);
	server->config_file = NULL;
    }
    my_CFRelease(&server->options);
    my_CFRelease(&server->enabled_interfaces);
    if (server->if_indices != NULL) {
	free(server->if_indices);
	server->if_indices = NULL;
    }
    if (server->if_names != NULL) {
	free(server->if_names);
	server->if_names = NULL;
    }
    my_CFRelease(&server->duid);
    if (server->store != NULL) {
	SCDynamicStoreSetDispatchQueue(server->store, NULL);
	my_CFRelease(&server->store);
    }
    *server_p = NULL;
    free(server);
    return;
}

STATIC int
S_send_packet(int sock_fd, IFIndex if_index, const struct in6_addr * dst_p,
	      DHCPv6PacketRef pkt, int pkt_size)
{
    struct sockaddr_in6	dst;

    dst = blank_sin6;
    dst.sin6_addr = *dst_p;
    dst.sin6_port = htons(S_client_port);
    return (IPv6SocketSend(sock_fd, if_index, &dst, pkt, pkt_size, -1));
}

STATIC int
DHCPv6ServerTransmit(DHCPv6ServerRef server,
		     IFIndex if_index,
		     const struct in6_addr * dst_p,
		     DHCPv6PacketRef pkt, int pkt_len)
{
    DHCPv6OptionErrorString	err;
    char			if_name[IFNAMSIZ];
    char 			ntopbuf[INET6_ADDRSTRLEN];
    int				ret;

    if (S_verbose) {
	DHCPv6OptionListRef	options;
	CFMutableStringRef	str;
	
	str = CFStringCreateMutable(NULL, 0);
	DHCPv6PacketPrintToString(str, pkt, pkt_len);
	options = DHCPv6OptionListCreateWithPacket(pkt, pkt_len, &err);
	if (options == NULL) {
	    my_log(LOG_NOTICE, "parse options failed, %s", err.str);
	}
	else {
	    DHCPv6OptionListPrintToString(str, options);
	    DHCPv6OptionListRelease(&options);
	}
	my_log(~LOG_NOTICE, "[%s] Transmit [%d bytes] to %s %@",
	       if_indextoname(if_index, if_name),
	       pkt_len,
	       inet_ntop(AF_INET6, dst_p, ntopbuf, sizeof(ntopbuf)),
	       str);
	CFRelease(str);
    }
    else {
	my_log(LOG_NOTICE, "[%s] Transmit %s (%d) [%d bytes] to %s",
	       if_indextoname(if_index, if_name),
	       DHCPv6MessageName(pkt->msg_type),
	       pkt->msg_type,
	       pkt_len,
	       inet_ntop(AF_INET6, dst_p, ntopbuf, sizeof(ntopbuf)));
    }
    ret = S_send_packet(server->sock_fd, if_index, dst_p, pkt, pkt_len);
    return (ret);
}
