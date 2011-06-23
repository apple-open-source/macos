/*
 * Copyright (c) 2003-2008 Apple Inc. All rights reserved.
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/uio.h>
#include <sys/time.h>
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
#include <arpa/inet.h>
#include <syslog.h>
#include <CoreFoundation/CFSocket.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "configthreads_common.h"
#include "globals.h"
#include "timer.h"
#include "ip6config_utils.h"

#ifdef RTADV_DEBUG
#define LOG_DEBUG LOG_ERR
#endif

/* Purpose: Registers a function to call when the descriptor is ready */
typedef void (CFCallout_func_t)(void * arg1, void * arg2);

/*
 * Type: rtadv_receive_func_t
 * Purpose:
 *   Called to deliver data to the client.  The first two args are
 *   supplied by the client, the third is a pointer to a rtadv_receive_data_t.
 */
typedef void (rtadv_receive_func_t)(void * arg1, void * arg2, void * arg3);

typedef struct {
    CFRunLoopSourceRef	rls;
    CFSocketRef		socket;
    CFCallout_func_t *	func;
    void *		arg1;
    void *		arg2;
} rtadv_callout_t;

#define RTSOL_PACKET_MAX (sizeof(struct nd_router_solicit) + MAX_LINK_ADDR_LEN)

typedef struct {
    int				sockfd;
    rtadv_callout_t *		callout;
    int				rs_datalen; /* amount actually used */
    char			rs_data[RTSOL_PACKET_MAX];
    rtadv_receive_func_t *	receive;
    void *			receive_arg1;
    void *			receive_arg2;
} rtadv_client_t;

typedef struct {
    int			retries;
    int			wait_secs;
    timer_callout_t *	timer;
    rtadv_client_t	client;
    short		llocal_flags;
    struct in6_addr	our_router;
} Service_rtadv_t;

static struct msghdr rcvmhdr;
static struct msghdr sndmhdr;
static struct iovec rcviov[2];
static struct iovec sndiov[2];
static struct sockaddr_in6 from;
static struct sockaddr_in6 sin6_allrouters = {sizeof(sin6_allrouters),
					      AF_INET6, 0, 0,
					      IN6ADDR_LINKLOCAL_ALLROUTERS_INIT, 0};

/* the intervals are 1, 2, 4, 8, 8, 8, etc until max solicitations (approx. 1 min) */
#define MAX_RTR_SOLICITATIONS		10 /* times */
#define MAX_WAIT_SECS			8 /* seconds */
#define ALLROUTER "ff02::2"

static void	rtadv_start(Service_t * service_p, IFEventID_t event_id, void * event_data);
static void	rtadv_read(CFSocketRef s, CFSocketCallBackType type, CFDataRef address,
			    const void *data, void *info);
static void	rtadv_link_timer(void * arg0, void * arg1, void * arg2);

static int
rtsol_make_packet(interface_t * if_p, rtadv_client_t * client)
{
    struct nd_router_solicit *	rs;
    size_t	packlen = sizeof(struct nd_router_solicit), lladdroptlen = 0;

	if ((lladdroptlen = lladdropt_length(&if_p->link_address)) == 0) {
	    my_log(LOG_DEBUG,
		   "rtsol_make_packet(%s): link-layer address option"
		   " has zero length. Treat as not included.",
		if_p->name);
    }
    packlen += lladdroptlen;
    client->rs_datalen = packlen;

    /* fill in the message */
	rs = (struct nd_router_solicit *)(client->rs_data);
    rs->nd_rs_type = ND_ROUTER_SOLICIT;
    rs->nd_rs_code = 0;
    rs->nd_rs_cksum = 0;
    rs->nd_rs_reserved = 0;

    /* fill in source link-layer address option */
    if (lladdroptlen) {
	    lladdropt_fill(&if_p->link_address, (struct nd_opt_hdr *)(rs + 1));
    }

    return(0);
}

static void
rtsol_sendpacket(Service_t * service_p)
{
    interface_t *	if_p = service_interface(service_p);
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)service_p->private;
    rtadv_client_t	client = rtadv->client;
    int			i;
    struct cmsghdr	*cm;
    struct in6_pktinfo	*pi;

    sndmhdr.msg_name = (caddr_t)&sin6_allrouters;
    sndmhdr.msg_iov[0].iov_base = (caddr_t)client.rs_data;
    sndmhdr.msg_iov[0].iov_len = client.rs_datalen;

    cm = CMSG_FIRSTHDR(&sndmhdr);
    /* specify the outgoing interface */
    cm->cmsg_level = IPPROTO_IPV6;
    cm->cmsg_type = IPV6_PKTINFO;
    cm->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
    pi = (struct in6_pktinfo *)CMSG_DATA(cm);
    memset(&pi->ipi6_addr, 0, sizeof(pi->ipi6_addr));	/*XXX*/
    pi->ipi6_ifindex = if_p->link_address.index;

    /* specify the hop limit of the packet */
    {
	int hoplimit = 255;

	cm = CMSG_NXTHDR(&sndmhdr, cm);
	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_HOPLIMIT;
	cm->cmsg_len = CMSG_LEN(sizeof(int));
	memcpy(CMSG_DATA(cm), &hoplimit, sizeof(int));
    }

    my_log(LOG_DEBUG, "rtsol_sendpacket: send RS on %s", if_p->name);

    i = sendmsg(client.sockfd, &sndmhdr, 0);

    if (i < 0 || i != client.rs_datalen) {
	/*
	 * ENETDOWN is not so serious, especially when using several
	 * network cards on a mobile node. We ignore it.
	 */
	if (errno != ENETDOWN) {
	    my_log(LOG_DEBUG, "rtsol_sendpacket: sendmsg on %s: %s",
		    if_p->name, strerror(errno));
	}
    }

    return;
}

static int
rtadv_verify_packet(interface_t * if_p)
{
    int			*hlimp = NULL;
    struct icmp6_hdr	*icp;
    int			ifindex = 0;
    struct cmsghdr	*cm;
    struct in6_pktinfo	*pi = NULL;
    struct nd_router_advert * ndra_p;
    char 		ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];

    /* extract optional information via Advanced API */
    for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(&rcvmhdr);
	    cm;
	    cm = (struct cmsghdr *)CMSG_NXTHDR(&rcvmhdr, cm)) {
	if (cm->cmsg_level == IPPROTO_IPV6 &&
		cm->cmsg_type == IPV6_PKTINFO &&
		cm->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo))) {
	    pi = (struct in6_pktinfo *)(CMSG_DATA(cm));
	    ifindex = pi->ipi6_ifindex;
	}
	if (cm->cmsg_level == IPPROTO_IPV6 &&
		cm->cmsg_type == IPV6_HOPLIMIT &&
		cm->cmsg_len == CMSG_LEN(sizeof(int)))
	    hlimp = (int *)CMSG_DATA(cm);
    }

    /* skip if not our interface */
	if (if_p->link_address.index != ifindex) {
	return (-1);
    }

    if (hlimp == NULL) {
	my_log(LOG_ERR, "RTADV_VERIFY_PACKET: failed to get receiving hop limit");
	return (-1);
    }

    icp = (struct icmp6_hdr *)rcvmhdr.msg_iov[0].iov_base;

    if (icp->icmp6_type != ND_ROUTER_ADVERT) {
	my_log(LOG_ERR, "RTADV_VERIFY_PACKET: invalid icmp type(%d) from %s on %s",
		icp->icmp6_type,
		inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf, sizeof(ntopbuf)),
		if_indextoname(pi->ipi6_ifindex, ifnamebuf));
	return (-1);
    }

    if (icp->icmp6_code != 0) {
	my_log(LOG_ERR, "RTADV_VERIFY_PACKET: invalid icmp code(%d) from %s on %s",
		icp->icmp6_code,
		inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf, sizeof(ntopbuf)),
		if_indextoname(pi->ipi6_ifindex, ifnamebuf));
	return (-1);
    }
    if (*hlimp != 255) {
	my_log(LOG_ERR, "RTADV_VERIFY_PACKET: invalid RA with hop limit(%d) from %s on %s",
		*hlimp,
		inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf, sizeof(ntopbuf)),
		if_indextoname(pi->ipi6_ifindex, ifnamebuf));
	return (-1);
    }

    if (pi && !IN6_IS_ADDR_LINKLOCAL(&from.sin6_addr)) {
	my_log(LOG_ERR, "RTADV_VERIFY_PACKET: invalid RA with non link-local source from %s on %s",
		inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf, sizeof(ntopbuf)),
		if_indextoname(pi->ipi6_ifindex, ifnamebuf));
	return (-1);
    }
    ndra_p = (struct nd_router_advert *)(icp);
    if (ndra_p->nd_ra_router_lifetime == 0) {
	/* ignore RA with lifetime zero */
	return (-1);
    }
    my_log(LOG_DEBUG, "RTADV_VERIFY_PACKET: received RA from %s on %s",
	    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf, sizeof(ntopbuf)),
	    if_p->name);

    return (0);
}

static void
rtadv_enable_receive(rtadv_client_t * client,
	    rtadv_receive_func_t * func,
	    void * arg1, void * arg2)
{
    client->receive = func;
    client->receive_arg1 = arg1;
    client->receive_arg2 = arg2;
    return;
}

static void
rtadv_disable_receive(rtadv_client_t * client)
{
    client->receive = NULL;
    client->receive_arg1 = NULL;
    client->receive_arg2 = NULL;
    return;
}

static int
rtsol_init_rcv_buffs(void)
{
    static u_char 	answer[1500];
    int 		rcvcmsglen;
    static u_char *	rcvcmsgbuf = NULL;

    rcvcmsglen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
		    CMSG_SPACE(sizeof(int));
    if (rcvcmsgbuf == NULL && (rcvcmsgbuf = malloc(rcvcmsglen)) == NULL) {
	my_log(LOG_DEBUG,
		"rtsol_init_rcv_buffs: malloc for receive msghdr failed");
	return (-1);
    }

    /* initialize msghdr for receiving packets */
    rcviov[0].iov_base = (caddr_t)answer;
    rcviov[0].iov_len = sizeof(answer);
    rcvmhdr.msg_name = (caddr_t)&from;
    rcvmhdr.msg_namelen = sizeof(from);
    rcvmhdr.msg_iov = rcviov;
    rcvmhdr.msg_iovlen = 1;
    rcvmhdr.msg_control = (caddr_t) rcvcmsgbuf;
    rcvmhdr.msg_controllen = rcvcmsglen;

    return(0);
}

static int
rtsol_init_snd_buffs(void)
{
    int			sndcmsglen;
    static u_char *	sndcmsgbuf = NULL;

    sndcmsglen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
		    CMSG_SPACE(sizeof(int));

    if (sndcmsgbuf == NULL && (sndcmsgbuf = malloc(sndcmsglen)) == NULL) {
	my_log(LOG_DEBUG,
		"rtsol_init_snd_buffs: malloc for send msghdr failed");
	return (-1);
    }

    /* initialize msghdr for sending packets */
    sndmhdr.msg_namelen = sizeof(struct sockaddr_in6);
    sndmhdr.msg_iov = sndiov;
    sndmhdr.msg_iovlen = 1;
    sndmhdr.msg_control = (caddr_t)sndcmsgbuf;
    sndmhdr.msg_controllen = sndcmsglen;

    return (0);
}

static int
init_sendbuffs_once()
{
    static int done = 0;

    if (done) {
	return (0);
    }

    /* init send buffers */
    if (rtsol_init_snd_buffs()) {
	return (-1);
    }
    done = 1;
    return (0);
}

static int
init_rtsol(int sockfd)
{
    int	on;
    struct icmp6_filter	filt;

    if (init_sendbuffs_once() < 0) {
	return (-1);
    }
    on = 1;
    if (ioctl(sockfd, FIONBIO, &on) < 0) {
	my_log(LOG_INFO, "init_rtsol: ioctl(FIONBIO): %s",
		strerror(errno));
	return (-1);
    }

    on = 1;
    if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_PKTINFO, &on,
		   sizeof(on)) < 0) {
	my_log(LOG_INFO, "init_rtsol: IPV6_PKTINFO: %s",
		strerror(errno));
	return (-1);
    }

    on = 1;
    if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_HOPLIMIT, &on,
		   sizeof(on)) < 0) {
	my_log(LOG_INFO, "init_rtsol: IPV6_HOPLIMIT: %s",
		strerror(errno));
	return (-1);
    }

    /* specify to accept only router advertisements on the socket */
    ICMP6_FILTER_SETBLOCKALL(&filt);
    ICMP6_FILTER_SETPASS(ND_ROUTER_ADVERT, &filt);
    if (setsockopt(sockfd, IPPROTO_ICMPV6, ICMP6_FILTER, &filt,
		   sizeof(filt)) == -1) {
	my_log(LOG_INFO, "init_rtsol: setsockopt(ICMP6_FILTER): %s",
		strerror(errno));
	return (-1);
    }
    return (0);
}

/* called by CFRunLoop stuff */
static void
rtadv_read(CFSocketRef s, CFSocketCallBackType type,
	      CFDataRef address, const void *data, void *info)
{
    rtadv_callout_t *	callout = (rtadv_callout_t *)info;
    rtadv_client_t *	client = (rtadv_client_t *)callout->arg1;
    int			n;

	/* initialize the receive buffer */
	if (rtsol_init_rcv_buffs() != 0) {
		my_log(LOG_ERR, "rtadv_read: error initializing receive buffs");
		return;
	}

    /* get message */
    n = recvmsg(client->sockfd, &rcvmhdr, 0);
    if (n < 0) {
	if (errno != EAGAIN) {
	    my_log(LOG_ERR, "rtadv_read(): recvfrom %d: %s",
	    errno, strerror(errno));
	}
    }
    else if (n > 0) {
	if (n < sizeof(struct nd_router_advert)) {
	    my_log(LOG_ERR, "rtadv_read(): packet size(%d) is too short", n);
	    return;
	}

	if (client->receive) {
	    (*client->receive)(client->receive_arg1, client->receive_arg2, NULL);
	}
    }

    return;
}

static int
rtadv_init_CFSocket(rtadv_client_t * client)
{
    CFSocketContext	context = { 0, NULL, NULL, NULL, NULL };

    if (client == NULL) {
	return (-1);
    }
    client->callout = calloc(1, sizeof(*client->callout));
    if (client->callout == NULL) {
	return (-1);
    }

    context.info = client->callout;
    client->callout->socket = CFSocketCreateWithNative(NULL, client->sockfd,
							kCFSocketReadCallBack,
							rtadv_read, &context);
    client->callout->rls = CFSocketCreateRunLoopSource(NULL, client->callout->socket, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), client->callout->rls, kCFRunLoopDefaultMode);

    client->callout->func = (CFCallout_func_t *)rtadv_read;
    client->callout->arg1 = client;
    client->callout->arg2 = NULL;

    return (0);
}

static void
rtadv_free_CFSocket(rtadv_client_t * client)
{
    if (client) {
	if (client->callout) {
	    /*free callout and CFSocket/RunLoop stuff*/
	    if (client->callout->rls) {
		/* cancel further callouts */
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), client->callout->rls,
					kCFRunLoopDefaultMode);

		/* remove one socket reference, close the file descriptor */
		CFSocketInvalidate(client->callout->socket);

		/* release the socket */
		CFRelease(client->callout->socket);
		client->callout->socket = NULL;

		/* release the run loop source */
		CFRelease(client->callout->rls);
		client->callout->rls = NULL;
	    }
	    free(client->callout);
	    client->callout = NULL;
	}
    }

    return;
}

static int
rtadv_client_init(rtadv_client_t * client)
{
    int	s, opt;

    if (client == NULL) {
	return (-1);
    }

    /* open socket */
    if ((s = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0) {
	my_log(LOG_INFO, "rtadv_client_init: error opening socket: %s",
		strerror(errno));
	return (-1);
    }

    /* set to non-blocking */
    opt = 1;
    if (ioctl(s, FIONBIO, &opt) < 0) {
	my_log(LOG_DEBUG, "rtadv_client_init: ioctl FIONBIO failed %s",
		strerror(errno));
	close(s);
	return (-1);
    }

    client->sockfd = s;

    if (rtadv_init_CFSocket(client)) {
	my_log(LOG_INFO, "rtadv_client_init: error initializing CFSocket");
	client->sockfd = -1;
	close(s);
	return (-1);
    }

    if (init_rtsol(client->sockfd) != 0) {
	client->sockfd = -1;
	close(s);
	return (-1);
    }

    return (0);
}

/* clean up client resources */
static void
rtadv_client_cleanup(rtadv_client_t * client)
{
    rtadv_free_CFSocket(client);

    if (client->sockfd != -1) {
	close(client->sockfd);
	client->sockfd = -1;
    }

    return;
}

static void
rtadv_cancel_pending_events(Service_t * service_p)
{
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)service_p->private;

    if (rtadv == NULL)
	return;
    if (rtadv->timer) {
	timer_cancel(rtadv->timer);
    }

    rtadv_disable_receive(&rtadv->client);

    return;
}

/* used when there's a problem with the link */
static void
rtadv_inactive(Service_t * service_p)
{
    rtadv_client_t	*client = &((Service_rtadv_t *)service_p->private)->client;

    rtadv_cancel_pending_events(service_p);
    rtadv_client_cleanup(client);
    service_remove_addresses(service_p);
    service_publish_failure(service_p, ip6config_status_media_inactive_e,
			    NULL);
    return;
}

static void
flush_prefixes()
{
    char	dummyif[IFNAMSIZ+8];
    int		s = inet6_dgram_socket();

    if (s < 0) {
	return;
    }
    strcpy(dummyif, "lo0"); /* dummy */
    /* this currently has a global effect */
    if (ioctl(s, SIOCSPFXFLUSH_IN6, (caddr_t)&dummyif) < 0) {
	my_log(LOG_DEBUG, "RTADV: error flushing prefixes");
	close(s);
	return;
    }
    my_log(LOG_DEBUG, "RTADV: flushed prefixes");
    close(s);
    return;
}

static void
flush_routes()
{
    char	dummyif[IFNAMSIZ+8];
    int		s = inet6_dgram_socket();

    if (s < 0) {
	return;
    }
    strcpy(dummyif, "lo0"); /* dummy */
    /* this currently has a global effect */
    if (ioctl(s, SIOCSRTRFLUSH_IN6, (caddr_t)&dummyif) < 0) {
	my_log(LOG_DEBUG, "RTADV: error flushing routes");
	close(s);
	return;
    }
    my_log(LOG_DEBUG, "RTADV: flushed routes");
    close(s);
    return;
}

static void
rtadv_link_timer(void * arg0, void * arg1, void * arg2)
{
    flush_prefixes();
    flush_routes();
    rtadv_inactive((Service_t *)arg0);
    return;
}

/* used when something goes wrong with setup */
static void
rtadv_failed(Service_t * service_p, ip6config_status_t status, char * msg)
{
    rtadv_cancel_pending_events(service_p);

    service_remove_addresses(service_p);
    service_publish_failure(service_p, status, msg);
    return;
}

/* send ioctl to start accepting rtadvs */
static int
accept_router_adv(interface_t *	if_p)
{
    int	s = inet6_dgram_socket();
    struct in6_ifreq	ifr;

    if (s < 0) {
	return (-1);
    }
    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name(if_p), sizeof(ifr.ifr_name));
    if (ioctl(s, SIOCAUTOCONF_START, &ifr) < 0) {
	close(s);
	return (-1);
    }
    my_log(LOG_DEBUG, "RTADV_START %s: rtadv enabled OK", if_name(if_p));
    close(s);
    return(0);
}

/* send ioctl to stop accepting rtadvs */
static int
disable_router_adv(interface_t * if_p)
{
    int	s = inet6_dgram_socket();
    struct in6_ifreq	ifr;

    if (s < 0) {
	return (-1);
    }
    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name(if_p), sizeof(ifr.ifr_name));
    if (ioctl(s, SIOCAUTOCONF_STOP, &ifr) < 0) {
	close(s);
	return (-1);
    }
    close(s);
    return (0);
}

static void
rtadv_start(Service_t * service_p, IFEventID_t event_id, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)service_p->private;
    ip6config_status_t	status = ip6config_status_success_e;
    struct timeval	tv;

    switch (event_id) {
	case IFEventID_start_e: {
	    rtadv->retries = 0;
	    rtadv->wait_secs = 1;
	    rtadv_cancel_pending_events(service_p);

	    /* make packet data */
	    (void)rtsol_make_packet(if_p, &rtadv->client);

	    /* forwarding not allowed */
	    if (getinet6sysctl(IPV6CTL_FORWARDING)) {
		my_log(LOG_DEBUG, "RTADV_START %s: kernel is configured as a router, not a host",
			if_name(if_p));
		status = ip6config_status_internal_error_e;
		rtadv_failed(service_p, status, "START");
		return;
		}
	    else {
		my_log(LOG_DEBUG, "RTADV_START %s: forwarding OK", if_name(if_p));
	    }

	    if(accept_router_adv(if_p)) {
		my_log(LOG_INFO, "RTADV_START %s: error accepting router advertisements",
			if_name(if_p));
		status = ip6config_status_internal_error_e;
		rtadv_failed(service_p, status, "START");
		return;
	    }

	    /* init client socket */
	    if (rtadv->client.sockfd < 0) {
		if (rtadv_client_init(&rtadv->client)) {
		    my_log(LOG_DEBUG, "RTADV %s: client init failed",
			    if_name(if_p));
		    status = ip6config_status_internal_error_e;
		    return;
		}
	    }

	    /* set client->receive, which processes incoming packets */
	    rtadv_enable_receive(&rtadv->client, (rtadv_receive_func_t *)rtadv_start,
				service_p, (void *)IFEventID_data_e);


	    /* FALL THROUGH */
	}
	case IFEventID_timeout_e: {
	    short	curr_flags;

	    if (rtadv->retries > 0) {
		if (service_link_status(service_p)->valid == TRUE
		    && service_link_status(service_p)->active == FALSE) {
		    my_log(LOG_DEBUG, "RTADV_START %s: link inactive", if_name(if_p));
		    rtadv_inactive(service_p);
		    return;
		}
	    }

	    if (rtadv->retries > MAX_RTR_SOLICITATIONS) {
		/* now we just wait to see if something comes in */
		my_log(LOG_DEBUG, "RTADV_START %s: rtadv->retries > MAX_RTR_SOLICITATIONS",
		       if_name(if_p));
		status = ip6config_status_no_rtadv_response_e;
		return;
	    }

	    if(get_llocal_if_addr_flags(if_name(if_p), &curr_flags) != 0) {
		my_log(LOG_DEBUG, "RTADV_START %s: error getting linklocal flags",
		       if_name(if_p));
		rtadv_inactive(service_p);
		status = ip6config_status_internal_error_e;
		return;
	    }

	    if (!(curr_flags & IN6_IFF_NOTREADY)) {
		my_log(LOG_DEBUG, "RTADV_START: IF READY");
		/* this means the interface is ready, and now we're just checking
		 * to make sure the retry stuff is set properly
		 */
		if (rtadv->llocal_flags & IN6_IFF_NOTREADY) {
		    /* this means it was not ready before and has become ready now */
		    rtadv->retries = 0;
		}

		/* send packet because interface is ready */
		rtsol_sendpacket(service_p);
	    }

	    rtadv->llocal_flags = curr_flags;
	    rtadv->retries++;

	    /* set timer values and wait for responses */
	    tv.tv_sec = rtadv->wait_secs;
	    tv.tv_usec = random_range(0, USECS_PER_SEC - 1);
	    timer_set_relative(rtadv->timer, tv,
			       (timer_func_t *)rtadv_start,
			       service_p, (void *)IFEventID_timeout_e, NULL);

	    /* double the wait time until reach max */
	    rtadv->wait_secs *= 2;
	    if(rtadv->wait_secs > MAX_WAIT_SECS) {
		rtadv->wait_secs = MAX_WAIT_SECS;
	    }

	    break;
	}
	case IFEventID_data_e: {
	    /* verify packet */
	    if (rtadv_verify_packet(if_p) != 0) {
		my_log(LOG_DEBUG,
		       "RTADV %s: START packet not OK", if_name(if_p));
	    }
	    else {
		/* save the router */
		memcpy(&rtadv->our_router, &from.sin6_addr, sizeof(struct in6_addr));

		/* we're not publishing here, we publish when we get
		 * notification from the kernel event monitor
		 */
		rtadv_cancel_pending_events(service_p);
	    }

	    break;
	}
	default:
	    break;
    }

	return;
}

__private_extern__ ip6config_status_t
rtadv_thread(Service_t * service_p, IFEventID_t evid, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    ip6config_status_t	status = ip6config_status_success_e;
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)service_p->private;

    switch (evid) {
	case IFEventID_start_e: {
	    if (if_flags(if_p) & IFF_LOOPBACK) {
		status = ip6config_status_invalid_operation_e;
		break;
	    }
	    if (rtadv) {
		my_log(LOG_DEBUG, "RTADV %s: re-entering start state",
		       if_name(if_p));
		status = ip6config_status_internal_error_e;
		break;
	    }

	    rtadv = calloc(1, sizeof(*rtadv));
	    if (rtadv == NULL) {
		my_log(LOG_ERR, "RTADV %s: calloc failed",
		       if_name(if_p));
		status = ip6config_status_allocation_failed_e;
		break;
	    }

	    service_p->private = rtadv;
	    rtadv->client.sockfd = -1;

	    rtadv->timer = timer_callout_init();
	    if (rtadv->timer == NULL) {
		my_log(LOG_ERR, "RTADV %s: timer_callout_init failed",
		       if_name(if_p));
		status = ip6config_status_allocation_failed_e;
		goto stop;
	    }

	    my_log(LOG_DEBUG, "RTADV %s: starting", if_name(if_p));

	    rtadv_start(service_p, evid, NULL);

	    break;
	}
	stop:
	case IFEventID_stop_e: {
	    my_log(LOG_DEBUG, "RTADV %s: stop", if_name(if_p));

	    if (rtadv == NULL) {
		my_log(LOG_DEBUG, "RTADV %s: already stopped",
		       if_name(if_p));
		status = ip6config_status_internal_error_e;
		break;
	    }

	    if(disable_router_adv(if_p)) {
		my_log(LOG_DEBUG, "RTADV %s: error disabling rtadv",
		if_name(if_p));
	    }

	    /* remove IP6 addresses */
	    service_remove_addresses(service_p);

	    /* clean-up resources */
	    if (rtadv->timer) {
		timer_callout_free(&rtadv->timer);
	    }

	    rtadv_client_cleanup(&rtadv->client);

	    /* flush prefixes and routes we may have acquired */
	    flush_prefixes();
	    flush_routes();

	    free(rtadv);
	    service_p->private = NULL;
	    break;
	}
	case IFEventID_state_change_e: {
	    int	i, count = 0;
	    ip6_addrinfo_list_t *	ip6_addrs = ((ip6_addrinfo_list_t *)event_data);
	    ip6_addrinfo_t		tmp_addrs[ip6_addrs->n_addrs];

	    if (rtadv == NULL) {
		my_log(LOG_DEBUG, "RTADV %s: private data is NULL",
		       if_name(if_p));
		status = ip6config_status_internal_error_e;
		break;
	    }

	    /* if we've suddenly lost our addresses
	     * and the link is still up, then
	     * someone probably flushed the prefixes;
	     * restart rtsols
	     */
	    if (ip6_addrs->n_addrs == 1 &&
		IN6_IS_ADDR_LINKLOCAL(&(ip6_addrs->addr_list[0].addr))) {
		if (service_link_status(service_p)->valid == TRUE) {
		    if (service_link_status(service_p)->active == TRUE) {
			my_log(LOG_DEBUG, "RTADV: RESTARTING rtsols on %s",
			       if_name(if_p));
			rtadv_start(service_p, IFEventID_start_e, NULL);
		    }
		}

		break;
	    }

	    /* only copy autoconf addresses */
	    for (i = 0; i < ip6_addrs->n_addrs; i++) {
		ip6_addrinfo_t	*new_addr = ip6_addrs->addr_list + i;

		if (new_addr->flags & IN6_IFF_AUTOCONF) {
		    memcpy(&tmp_addrs[count].addr, &new_addr->addr, sizeof(struct in6_addr));
		    tmp_addrs[count].prefixlen = new_addr->prefixlen;
		    tmp_addrs[count].flags = new_addr->flags;
		    prefixLen2mask(&tmp_addrs[count].prefixmask,
				   tmp_addrs[count].prefixlen);
		    count++;
		}
	    }

	    if (!count) {
		my_log(LOG_DEBUG, "RTADV: no rtadv addresses found for interface %s",
		       if_name(if_p));
		break;  // this is a strange failure, but not catastrophic
	    }

	    if (service_p->info.addrs.addr_list) {
		free(service_p->info.addrs.addr_list);
		service_p->info.addrs.addr_list = NULL;
		service_publish_clear(service_p);
	    }

	    service_p->info.addrs.addr_list = malloc(count * sizeof(ip6_addrinfo_t));
	    if (service_p->info.addrs.addr_list == NULL) {
		my_log(LOG_ERR, "RTADV: error allocating memory for addresses on interface %s",
		       if_name(if_p));
		status = ip6config_status_allocation_failed_e;
		break;
	    }
	    service_p->info.addrs.n_addrs = count;
	    memcpy(service_p->info.addrs.addr_list, &tmp_addrs, count * sizeof(ip6_addrinfo_t));

	    /* copy the saved router info */
	    memcpy(&service_p->info.router, &rtadv->our_router, sizeof(struct in6_addr));

	    service_publish_success(service_p);

	    break;
	}
	case IFEventID_media_e: {
	    if (rtadv == NULL)
		return (ip6config_status_internal_error_e);

	    if (service_link_status(service_p)->valid == TRUE) {
		if (service_link_status(service_p)->active == TRUE) {
		    service_remove_addresses(service_p);
		    rtadv_start(service_p, IFEventID_start_e, NULL);
		}
		else {
		    struct timeval tv;

		    /* if link goes down and stays down long enough, unpublish */
		    rtadv_cancel_pending_events(service_p);
		    tv.tv_sec = LINK_INACTIVE_WAIT_SECS;
		    tv.tv_usec = 0;
		    timer_set_relative(rtadv->timer, tv,
				       (timer_func_t *)rtadv_link_timer,
				       service_p, NULL, NULL);
		}
	    }
	    break;
	}
	default:
	    break;
    } /* switch */

    return (status);
}
