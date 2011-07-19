/*
 * Copyright (c) 2000-2010 Apple Inc. All rights reserved.
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
 * rtutil.c
 * - routing table routines
 */

/* 
 * Modification History
 *
 * June 23, 2009	Dieter Siegmund (dieter@apple.com)
 * - split out from ipconfigd.c
 */


#include "rtutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <net/route.h>
#include <net/if_dl.h>
#define KERNEL_PRIVATE
#include <sys/ioctl.h>
#undef KERNEL_PRIVATE
#include <mach/boolean.h>
#include "symbol_scope.h"
#include "util.h"
#include "arp.h"
#include "mylog.h"
#include "globals.h"

STATIC void
set_sockaddr_in(struct sockaddr_in * sin_p, struct in_addr iaddr)
{
    sin_p->sin_len = sizeof(*sin_p);
    sin_p->sin_family = AF_INET;
    sin_p->sin_addr = iaddr;
}

PRIVATE_EXTERN boolean_t
host_route(int cmd, struct in_addr iaddr)
{
    int 			len;
    struct in_addr		loopback = { htonl(INADDR_LOOPBACK) };
    boolean_t			ret = TRUE;
    struct rt_msghdr *		rtm;
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
    rtm = &rtmsg.hdr;
    rtm->rtm_type = cmd;
    rtm->rtm_flags = RTF_UP | RTF_STATIC | RTF_HOST;
    rtm->rtm_version = RTM_VERSION;
    rtm->rtm_seq = arp_get_next_seq();
    rtm->rtm_addrs = RTA_DST | RTA_GATEWAY;
    set_sockaddr_in(&rtmsg.dst, iaddr);
    set_sockaddr_in(&rtmsg.gway, loopback);

    len = sizeof(rtmsg);
    rtm->rtm_msglen = len;
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

#define ROUNDUP(a) \
    ((a) > 0 ? (1 + (((a) - 1) | (sizeof(u_int32_t) - 1))) : sizeof(u_int32_t))

PRIVATE_EXTERN int
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

/*
 * Function: subnet_route_if_index
 * Purpose:
 *   Look up the subnet route for the given subnet.
 * Returns:
 *   Non-zero interface index if the route was found, zero otherwise.
 *
 */
PRIVATE_EXTERN int
subnet_route_if_index(struct in_addr netaddr, struct in_addr netmask)
{
    struct sockaddr_in *	addr_p;
    int 			len;
    int				n;
    char *			ptr;
    int				pid;
    boolean_t			ret_if_index = 0;
    struct rt_msghdr *		rtm;
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
    rtm = &rtmsg.hdr;
    rtm->rtm_type = RTM_GET_SILENT;
    rtm->rtm_version = RTM_VERSION;
    rtm->rtm_seq = rtm_seq = arp_get_next_seq();
    rtm->rtm_addrs = RTA_DST | RTA_NETMASK;
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
    rtm->rtm_msglen = len;
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
    pid = getpid();
    while ((n = read(sockfd, &rtmsg, sizeof(rtmsg))) > 0) {
	struct rt_addrinfo	info;

	if (rtm->rtm_type != RTM_GET
	    || rtm->rtm_seq != rtm_seq || rtm->rtm_pid != pid) {
	    continue;
	}
	info.rti_addrs = rtm->rtm_addrs;
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


/*
 * Function: subnet_route
 * Purpose:
 *    Add/remove/get the specified subnet route.
 * Returns:
 *    TRUE if operation was successful, FALSE otherwise.
 */
STATIC boolean_t
subnet_route(int cmd, struct in_addr gateway, struct in_addr netaddr, 
	     struct in_addr netmask, const char * ifname)
{
    int 			len;
    boolean_t			ret = TRUE;
    struct rt_msghdr *		rtm;
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
    rtm = &rtmsg.hdr;
    rtm->rtm_type = cmd;
    rtm->rtm_flags = RTF_UP | RTF_STATIC | RTF_CLONING;
    rtm->rtm_version = RTM_VERSION;
    rtm->rtm_seq = arp_get_next_seq();
    rtm->rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
    set_sockaddr_in(&rtmsg.dst, netaddr);
    set_sockaddr_in(&rtmsg.gway, gateway);
    set_sockaddr_in(&rtmsg.mask, netmask);

    len = sizeof(rtmsg);
    if (ifname != NULL) {
	rtm->rtm_addrs |= RTA_IFP | RTA_IFA;
	/* copy the interface name */
	rtmsg.ifp.sdl_len = sizeof(rtmsg.ifp);
	rtmsg.ifp.sdl_family = AF_LINK;
	rtmsg.ifp.sdl_nlen = strlen(ifname);
	bcopy(ifname, rtmsg.ifp.sdl_data, rtmsg.ifp.sdl_nlen);
	/* and the interface address (which is the gateway) */
	set_sockaddr_in(&rtmsg.ifa, gateway);
    }
    else {
	/* no ifp/ifa information */
	len -= sizeof(rtmsg.ifp) + sizeof(rtmsg.ifa);
    }
    rtm->rtm_msglen = len;
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

PRIVATE_EXTERN boolean_t
subnet_route_add(struct in_addr gateway, struct in_addr netaddr, 
		 struct in_addr netmask, const char * ifname)
{
    return (subnet_route(RTM_ADD, gateway, netaddr, netmask, ifname));
}

PRIVATE_EXTERN boolean_t
subnet_route_delete(struct in_addr gateway, struct in_addr netaddr, 
		    struct in_addr netmask)
{
    return (subnet_route(RTM_DELETE, gateway, netaddr, netmask, NULL));
}


#define N_MIB		6

STATIC int
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
	rtm->rtm_seq = arp_get_next_seq();
	if (write(s, rtm, rtm->rtm_msglen) < 0) {
	    my_log(LOG_NOTICE,
		   "IPConfiguration: removing dynamic route for "
		   IP_FORMAT " failed, %s",
		   IP_LIST(&sin->sin_addr),
		   strerror(errno));
	}
	else if (G_IPConfiguration_verbose) {
	    my_log(LOG_NOTICE,
		   "IPConfiguration: removed dynamic route for " IP_FORMAT, 
		   IP_LIST(&sin->sin_addr));
	}
    }
    free(buf);
    return (0);
}

PRIVATE_EXTERN void
flush_routes(int if_index, const struct in_addr ip,
	     const struct in_addr broadcast) 
{
    int		s;

    s = arp_open_routing_socket();
    if (s < 0) {
	return;
    }

    /* remove permanent arp entries for the IP and IP broadcast.
     * - do these first because they require reading from the routing socket
     * - flushing only requires writing to the routing socket
     */
    if (ip.s_addr) { 
	(void)arp_delete(s, ip, 0);
    }
    if (broadcast.s_addr) { 
	(void)arp_delete(s, broadcast, 0);
    }

    /* blow away all non-permanent arp entries */
    (void)arp_flush(s, FALSE, if_index);

    (void)flush_dynamic_routes(s);
    close(s);
    return;
}
