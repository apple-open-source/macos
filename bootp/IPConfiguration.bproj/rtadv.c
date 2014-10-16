/*
 * Copyright (c) 2003-2014 Apple Inc. All rights reserved.
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
 * rtadv.c
 * - IPv6 Router Advertisement
 * - sends router solicitation requests, and waits for responses
 * - reads router advertisement messages, and from it, gleans the source
 *   IPv6 address to use as the Router
 */

/* 
 * Modification History
 *
 * October 6, 2009		Dieter Siegmund (dieter@apple.com)
 * - added support for DHCPv6
 */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/param.h>
#include <ctype.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#define KERNEL_PRIVATE
#include <netinet6/in6_var.h>
#undef KERNEL_PRIVATE
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <CoreFoundation/CFSocket.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "cfutil.h"
#include "ipconfigd_threads.h"
#include "FDSet.h"
#include "globals.h"
#include "timer.h"
#include "ifutil.h"
#include "util.h"
#include "symbol_scope.h"
#include "DHCPv6Client.h"
#include "RTADVSocket.h"

typedef struct {
    timer_callout_t *		timer;
    int				try;
    Boolean			data_received;
    struct in6_addr		our_router;
    uint8_t			our_router_hwaddr[MAX_LINK_ADDR_LEN];
    int				our_router_hwaddr_len;
    RTADVSocketRef		sock;
    boolean_t			lladdr_ok; /* ok to send link-layer address */
    DHCPv6ClientRef		dhcp_client;
    struct in6_addr *		dns_servers;
    int				dns_servers_count;
    boolean_t			renew;
} Service_rtadv_t;


STATIC void
rtadv_set_dns_servers(Service_rtadv_t * rtadv,
		      const struct in6_addr * dns_servers,
		      int dns_servers_count)
{
    if (rtadv->dns_servers != NULL) {
	free(rtadv->dns_servers);
	rtadv->dns_servers = NULL;
    }
    if (dns_servers != NULL) {
	rtadv->dns_servers = (struct in6_addr *)
	    malloc(sizeof(*dns_servers) * dns_servers_count);
	bcopy(dns_servers, rtadv->dns_servers,
	      sizeof(*dns_servers) * dns_servers_count);
	rtadv->dns_servers_count = dns_servers_count;
    }
    return;
}

STATIC void
rtadv_cancel_pending_events(ServiceRef service_p)
{
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    timer_cancel(rtadv->timer);
    RTADVSocketDisableReceive(rtadv->sock);
    return;
}

STATIC void
rtadv_failed(ServiceRef service_p, ipconfig_status_t status)
{
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    rtadv->try = 0;
    rtadv_cancel_pending_events(service_p);
    inet6_rtadv_disable(if_name(service_interface(service_p)));
    rtadv_set_dns_servers(rtadv, NULL, 0);
    service_publish_failure(service_p, status);
    return;
}

STATIC void
rtadv_inactive(ServiceRef service_p)
{
    interface_t *	if_p = service_interface(service_p);

    inet6_flush_prefixes(if_name(if_p));
    inet6_flush_routes(if_name(if_p));
    rtadv_failed(service_p, ipconfig_status_media_inactive_e);
    return;
}

STATIC void
rtadv_start(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    RTADVSocketReceiveDataRef 	data;
    int				error;
    interface_t *		if_p = service_interface(service_p);
    char 			ntopbuf[INET6_ADDRSTRLEN];
    Service_rtadv_t *		rtadv;
    struct timeval		tv;

    rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    switch (event_id) {
    case IFEventID_start_e:
	my_log(LOG_DEBUG, "RTADV: start %s", if_name(if_p));
	rtadv_cancel_pending_events(service_p);
	RTADVSocketEnableReceive(rtadv->sock,
				 (RTADVSocketReceiveFuncPtr)rtadv_start,
				 service_p, (void *)IFEventID_data_e);
	if (inet6_rtadv_enable(if_name(if_p)) != 0) {
	    rtadv_failed(service_p, ipconfig_status_internal_error_e);
	    return;
	}
	bzero(&rtadv->our_router, sizeof(rtadv->our_router));
	rtadv_set_dns_servers(rtadv, NULL, 0);
	rtadv->our_router_hwaddr_len = 0;
	rtadv->try = 0;
	rtadv->data_received = FALSE;

	/* FALL THROUGH */

    case IFEventID_timeout_e:
	rtadv->try++;
	if (rtadv->try > 1) {
	    link_status_t	link_status = service_link_status(service_p);

	    if (link_status.valid == TRUE
		&& link_status.active == FALSE) {
		rtadv_inactive(service_p);
		return;
	    }
	}
	if (rtadv->try > MAX_RTR_SOLICITATIONS) {
	    /* now we just wait to see if something comes in */
	    return;
	}
	my_log(LOG_DEBUG, 
	       "RTADV %s: sending Router Solicitation (%d of %d)",
	       if_name(if_p), rtadv->try, MAX_RTR_SOLICITATIONS);
	error = RTADVSocketSendSolicitation(rtadv->sock,
					    rtadv->lladdr_ok);
	switch (error) {
	case 0:
	case ENXIO:
	case ENETDOWN:
	case EADDRNOTAVAIL:
	    break;
	default:
	    my_log(LOG_ERR, "RTADV %s: send Router Solicitation: failed, %s",
		   if_name(if_p), strerror(error));
	    break;
	}
	
	/* set timer values and wait for responses */
	tv.tv_sec = RTR_SOLICITATION_INTERVAL;
	tv.tv_usec = (suseconds_t)random_range(0, USECS_PER_SEC - 1);
	timer_set_relative(rtadv->timer, tv,
			   (timer_func_t *)rtadv_start,
			   service_p, (void *)IFEventID_timeout_e, NULL);
	break;

    case IFEventID_data_e:
	data = (RTADVSocketReceiveDataRef)event_data;
	/* save the router and flags, and start DHCPv6 if necessary */
	if (G_IPConfiguration_verbose) {
	    char		link_addr_buf[MAX_LINK_ADDR_LEN * 3 + 1];

	    link_addr_buf[0] = '\0';
	    if (data->router_hwaddr != NULL) {
		if (data->router_hwaddr_len == ETHER_ADDR_LEN) {
		    snprintf(link_addr_buf, sizeof(link_addr_buf),
			     " (" EA_FORMAT ")",
			     EA_LIST(data->router_hwaddr));
		}
		else if (data->router_hwaddr_len == 8) {
		    snprintf(link_addr_buf, sizeof(link_addr_buf),
			     " (" FWA_FORMAT ")",
			     FWA_LIST(data->router_hwaddr));
		}
	    }

	    my_log(LOG_DEBUG, 
		   "RTADV %s: Received RA from %s%s%s%s",
		   if_name(if_p),
		   inet_ntop(AF_INET6, &data->router,
			     ntopbuf, sizeof(ntopbuf)),
		   link_addr_buf,
		   data->managed_bit ? " [Managed]" : "",
		   data->other_bit ? " [OtherConfig]" : "");
	    if (data->dns_servers != NULL) {
		int		i;
		
		for (i = 0; i < data->dns_servers_count; i++) {
		    my_log(LOG_DEBUG, 
			   "RTADV %s: DNS Server %s",
			   if_name(if_p),
			   inet_ntop(AF_INET6, data->dns_servers + i,
				     ntopbuf, sizeof(ntopbuf)));
		}
	    }
	}
	rtadv->data_received = TRUE;
	rtadv_cancel_pending_events(service_p);
	rtadv->our_router = data->router;
	if (data->router_hwaddr != NULL) {
	    int		len;

	    len = data->router_hwaddr_len;
	    if (len > sizeof(rtadv->our_router_hwaddr)) {
		len = sizeof(rtadv->our_router_hwaddr);
	    }
	    bcopy(data->router_hwaddr, rtadv->our_router_hwaddr, len);
	    rtadv->our_router_hwaddr_len = len;
	}
	rtadv_set_dns_servers(rtadv, data->dns_servers, 
			      data->dns_servers_count);
	if (rtadv->dhcp_client != NULL) {
	    if (data->managed_bit || data->other_bit) {
		DHCPv6ClientStart(rtadv->dhcp_client,
				  (G_dhcpv6_stateful_enabled
				   && data->managed_bit));
	    }
	    else {
		DHCPv6ClientStop(rtadv->dhcp_client, FALSE);
	    }
	}
	break;
    default:
	break;
    }
    return;
}

STATIC CFStringRef
rtadv_create_signature(ServiceRef service_p,
		       inet6_addrinfo_t * list_p, int list_count)
{
    struct in6_addr	netaddr;
    char 		ntopbuf[INET6_ADDRSTRLEN];
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    CFMutableStringRef	sig_str;

    if (list_p == NULL || list_count == 0
	|| rtadv->our_router_hwaddr_len == 0) {
	return (NULL);
    }
    netaddr = list_p[0].addr;
    in6_netaddr(&netaddr, list_p[0].prefix_length);
    sig_str = CFStringCreateMutable(NULL, 0);
    CFStringAppendFormat(sig_str, NULL, 
			 CFSTR("IPv6.Prefix=%s/%d;IPv6.RouterHardwareAddress="),
			 inet_ntop(AF_INET6, &netaddr,
				   ntopbuf, sizeof(ntopbuf)),
			 list_p[0].prefix_length);
    my_CFStringAppendBytesAsHex(sig_str, rtadv->our_router_hwaddr,
				rtadv->our_router_hwaddr_len, ':');
    return (sig_str);
}

#ifndef IN6_IFF_SWIFTDAD
#define IN6_IFF_SWIFTDAD	0x0800  /* DAD with no delay */
#endif /* IN6_IFF_SWIFTDAD */

STATIC void
rtadv_trigger_dad(ServiceRef service_p, inet6_addrinfo_t * list, int count)
{
    int			i;
    interface_t *	if_p = service_interface(service_p);
    inet6_addrinfo_t *	scan;
    int			sockfd;

    sockfd = inet6_dgram_socket();
    if (sockfd < 0) {
	my_log(LOG_ERR,
	       "RTADV %s: failed to open socket, %s",
	       if_name(if_p), strerror(errno));
	return;
    }
    for (i = 0, scan = list; i < count; scan++, i++) {
	char 	ntopbuf[INET6_ADDRSTRLEN];

	if (inet6_aifaddr(sockfd, if_name(if_p),
			  &scan->addr, NULL, scan->prefix_length,
			  scan->addr_flags | IN6_IFF_SWIFTDAD,
			  scan->valid_lifetime,
			  scan->preferred_lifetime) < 0) {
	    my_log(LOG_ERR,
		   "RTADV %s: inet6_aifaddr(%s/%d) failed, %s",
		   if_name(if_p),
		   inet_ntop(AF_INET6, &scan->addr, ntopbuf, sizeof(ntopbuf)),
		   scan->prefix_length, strerror(errno));
	}
	else if (G_IPConfiguration_verbose) {
	    my_log(LOG_DEBUG,
		   "RTADV %s: Re-assigned %s/%d",
		   if_name(if_p),
		   inet_ntop(AF_INET6, &scan->addr, ntopbuf, sizeof(ntopbuf)),
		   scan->prefix_length);
	}
    }
    close(sockfd);
    return;
}

STATIC void
rtadv_address_changed(ServiceRef service_p,
		      inet6_addrlist_t * addr_list_p)
{
    interface_t *	if_p = service_interface(service_p);
    inet6_addrinfo_t *	linklocal;
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    boolean_t		try_was_zero = FALSE;

    linklocal = inet6_addrlist_get_linklocal(addr_list_p);
    if (linklocal == NULL) {
	/* no linklocal address assigned, nothing to do */
	my_log(LOG_DEBUG,
	       "RTADV %s: link-local address not present",
	       if_name(if_p));
	return;
    }
    if ((linklocal->addr_flags & IN6_IFF_NOTREADY) != 0) {
	if ((linklocal->addr_flags & IN6_IFF_DUPLICATED) != 0) {
	    /* address conflict occurred */
	    rtadv_failed(service_p, ipconfig_status_address_in_use_e);
	    return;
	}
	/* linklocal address isn't ready */
	my_log(LOG_DEBUG,
	       "RTADV %s: link-local address is not ready",
	       if_name(if_p));
	return;
    }
    rtadv->lladdr_ok = (linklocal->addr_flags & IN6_IFF_DADPROGRESS) == 0;
    my_log(LOG_DEBUG,
	   "RTADV %s: link-layer option in Router Solicitation is %sOK",
	   if_name(if_p), rtadv->lladdr_ok ? "" : "not ");
    if (rtadv->try == 0) {
	link_status_t	link_status = service_link_status(service_p);

	try_was_zero = TRUE;
	if (link_status.valid == FALSE
	    || link_status.active == TRUE) {
	    my_log(LOG_DEBUG,
		   "RTADV %s: link-local address is ready, starting",
		   if_name(if_p));
	    rtadv_start(service_p, IFEventID_start_e, NULL);
	}
    }
    if (rtadv->renew || !try_was_zero) {
	int			count;
	inet6_addrlist_t	dhcp_addr_list;
	int			i;
	dhcpv6_info_t		info;
	dhcpv6_info_t *		info_p = NULL;
	inet6_addrinfo_t *	scan;
	inet6_addrinfo_t	list[addr_list_p->count];
	struct in6_addr *	router = NULL;
	int			router_count = 0;
	CFStringRef		signature = NULL;

	inet6_addrlist_init(&dhcp_addr_list);
	if (rtadv->dhcp_client != NULL) {
	    DHCPv6ClientCopyAddresses(rtadv->dhcp_client, &dhcp_addr_list);
	}

	/* only copy autoconf and DHCP addresses */
	for (i = 0, count = 0, scan = addr_list_p->list; 
	     i < addr_list_p->count; i++, scan++) {
	    if ((scan->addr_flags & IN6_IFF_NOTREADY) != 0) {
		continue;
	    }
	    if ((scan->addr_flags & IN6_IFF_AUTOCONF) != 0
		|| inet6_addrlist_contains_address(&dhcp_addr_list, scan)) {
		list[count++] = *scan;
	    }
	}
	inet6_addrlist_free(&dhcp_addr_list);
	if (count == 0) {
	    return;
	}
	if (IN6_IS_ADDR_UNSPECIFIED(&rtadv->our_router) == FALSE) {
	    router = &rtadv->our_router;
	    router_count = 1;
	}
	if (rtadv->dhcp_client != NULL
	    && DHCPv6ClientGetInfo(rtadv->dhcp_client, &info)) {
	    info_p = &info;
	}
	if (rtadv->dns_servers != NULL) {
	    info.dns_servers = rtadv->dns_servers;
	    info.dns_servers_count = rtadv->dns_servers_count;
	    info_p = &info;
	}
	signature = rtadv_create_signature(service_p, list, count);
	ServicePublishSuccessIPv6(service_p, list, count, router, router_count,
				  info_p, signature);
	if (rtadv->renew) {
	    /* re-assign address to trigger DAD */
	    rtadv_trigger_dad(service_p, list, count);
	    rtadv->renew = FALSE;
	}
	my_CFRelease(&signature);
    }
    return;
}

STATIC void
rtadv_dhcp_callback(void * callback_arg, DHCPv6ClientRef client)
{
    inet6_addrlist_t	addrs;
    ServiceRef		service_p = (ServiceRef)callback_arg;

    inet6_addrlist_copy(&addrs, if_link_index(service_interface(service_p)));
    rtadv_address_changed(service_p, &addrs);
    inet6_addrlist_free(&addrs);
    return;
}

STATIC void
rtadv_init(ServiceRef service_p)
{
    inet6_addrlist_t	addrs;
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    rtadv->try = 0;
    inet6_addrlist_copy(&addrs,
			if_link_index(service_interface(service_p)));
    rtadv_address_changed(service_p, &addrs);
    inet6_addrlist_free(&addrs);
    return;
}

PRIVATE_EXTERN ipconfig_status_t
rtadv_thread(ServiceRef service_p, IFEventID_t evid, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    switch (evid) {
    case IFEventID_start_e:
	if (if_flags(if_p) & IFF_LOOPBACK) {
	    status = ipconfig_status_invalid_operation_e;
	    break;
	}
	if (rtadv != NULL) {
	    my_log(LOG_DEBUG, "RTADV %s: re-entering start state",
		   if_name(if_p));
	    status = ipconfig_status_internal_error_e;
	    break;
	}
	rtadv = malloc(sizeof(*rtadv));
	if (rtadv == NULL) {
	    my_log(LOG_ERR, "RTADV %s: malloc failed", if_name(if_p));
	    status = ipconfig_status_allocation_failed_e;
	    break;
	}
	bzero(rtadv, sizeof(*rtadv));
	ServiceSetPrivate(service_p, rtadv);
	rtadv->timer = timer_callout_init();
	if (rtadv->timer == NULL) {
	    my_log(LOG_ERR, "RTADV %s: timer_callout_init failed",
		   if_name(if_p));
	    status = ipconfig_status_allocation_failed_e;
	    goto stop;
	}
	rtadv->sock = RTADVSocketCreate(if_p);
	if (rtadv->sock == NULL) {
	    my_log(LOG_ERR, "RTADV %s: RTADVSocketCreate failed",
		   if_name(if_p));
	    status = ipconfig_status_allocation_failed_e;
	    goto stop;
	}
	if (G_dhcpv6_enabled) {
	    rtadv->dhcp_client = DHCPv6ClientCreate(if_p);
	    if (rtadv->dhcp_client == NULL) {
		my_log(LOG_ERR, "RTADV %s: DHCPv6ClientCreate failed",
		       if_name(if_p));
		status = ipconfig_status_allocation_failed_e;
		goto stop;
	    }
	    DHCPv6ClientSetNotificationCallBack(rtadv->dhcp_client,
						rtadv_dhcp_callback,
						service_p);
	}
	rtadv_init(service_p);
	break;

    stop:
    case IFEventID_stop_e:
	if (rtadv == NULL) {
	    my_log(LOG_DEBUG, "RTADV %s: already stopped",
		   if_name(if_p));
	    status = ipconfig_status_internal_error_e;
	    break;
	}
	my_log(LOG_DEBUG, "RTADV %s: stop", if_name(if_p));

	/* close/release the RTADV socket */
	RTADVSocketRelease(&rtadv->sock);

	/* stop DHCPv6 client */
	DHCPv6ClientRelease(&rtadv->dhcp_client);

	/* this flushes the addresses */
	(void)inet6_rtadv_disable(if_name(if_p));

	/* clean-up resources */
	if (rtadv->timer) {
	    timer_callout_free(&rtadv->timer);
	}
	rtadv_set_dns_servers(rtadv, NULL, 0);
	inet6_flush_prefixes(if_name(if_p));
	inet6_flush_routes(if_name(if_p));
	ServiceSetPrivate(service_p, NULL);
	free(rtadv);
	break;

    case IFEventID_ipv6_address_changed_e:
	if (rtadv == NULL) {
	    my_log(LOG_DEBUG, "RTADV %s: private data is NULL",
		   if_name(if_p));
	    status = ipconfig_status_internal_error_e;
	    break;
	}
	if (rtadv->dhcp_client != NULL) {
	    DHCPv6ClientAddressChanged(rtadv->dhcp_client, event_data);
	}
	rtadv_address_changed(service_p, event_data);
	break;
    case IFEventID_renew_e:
    case IFEventID_link_status_changed_e: {
	link_status_t	link_status;
	void *		network_changed = event_data;

	if (rtadv == NULL) {
	    return (ipconfig_status_internal_error_e);
	}
	link_status = service_link_status(service_p);
	if (link_status.valid == FALSE
	    || link_status.active == TRUE) {
	    if (network_changed != NULL) {
		inet6_flush_prefixes(if_name(if_p));
		inet6_flush_routes(if_name(if_p));
		inet6_rtadv_disable(if_name(if_p));
		if (rtadv->dhcp_client != NULL) {
		    DHCPv6ClientStop(rtadv->dhcp_client, TRUE);
		}
		service_publish_failure(service_p,
					ipconfig_status_network_changed_e);
	    }
	    else if (evid == IFEventID_link_status_changed_e
		     && rtadv->try == 1
		     && rtadv->data_received == FALSE) {
		/* we're already on it */
		break;
	    }
	    if (evid == IFEventID_renew_e 
		&& if_ift_type(if_p) == IFT_CELLULAR) {
		rtadv->renew = TRUE;
	    }
	    rtadv_init(service_p);
	}
	else {
	    rtadv->try = 0;
	}
	break;
    }
    case IFEventID_link_timer_expired_e:
	rtadv_inactive(service_p);
	if (rtadv->dhcp_client != NULL) {
	    DHCPv6ClientStop(rtadv->dhcp_client, FALSE);
	}
	break;

    case IFEventID_wake_e:
	break;

    case IFEventID_get_dhcpv6_info_e: {
	dhcpv6_info_t *		info_p = (dhcpv6_info_t *)event_data;

	if (rtadv->dhcp_client != NULL) {
	    (void)DHCPv6ClientGetInfo(rtadv->dhcp_client, info_p);
	}
	if (rtadv->dns_servers != NULL) {
	    info_p->dns_servers = rtadv->dns_servers;
	    info_p->dns_servers_count = rtadv->dns_servers_count;
	}
	break;
    }
    default:
	break;
    } /* switch */

    return (status);
}
