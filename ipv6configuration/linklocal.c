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
#include <stdio.h>
#include <stdarg.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#define KERNEL_PRIVATE
#include <netinet6/in6_var.h>
#undef KERNEL_PRIVATE
#include <syslog.h>

#include "timer.h"
#include "configthreads_common.h"
#include "ip6config_utils.h"

#ifdef LLOCAL_DEBUG
#define LOG_DEBUG LOG_ERR
#endif

typedef struct {
    timer_callout_t *	timer;
} Service_llocal_t;

typedef struct {
    struct rt_msghdr 	m_rtm;
    char		m_space[512];
} llocal_rtmsg_t;

static int
siocprotoattach(int s, char * name)
{
    struct in6_aliasreq		ifra;

    bzero(&ifra, sizeof(ifra));
    strncpy(ifra.ifra_name, name, sizeof(ifra.ifra_name));
    return (ioctl(s, SIOCPROTOATTACH_IN6, &ifra));
}

static int
siocprotodetach(int s, char * name)
{
    struct in6_ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCPROTODETACH_IN6, &ifr));
}

static int
linklocal_start(int s, char * name)
{
    struct in6_aliasreq	ifra_in6;

    bzero(&ifra_in6, sizeof(ifra_in6));
    strncpy(ifra_in6.ifra_name, name, sizeof(ifra_in6.ifra_name));
    return (ioctl(s, SIOCLL_START, &ifra_in6));
}

static int
linklocal_stop(int s, char * name)
{
    struct in6_ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCLL_STOP, &ifr));
}

/*
 * The following clears non-permanent routes added through
 * neighbor discovery
 * (from ndp.c)
 */
static const struct sockaddr_in6 blank_sin = {sizeof(blank_sin), AF_INET6 };

static int
send_rtmsg(int s, int cmd, llocal_rtmsg_t *m_rtmsg, struct sockaddr_in6 *sin_m)
{
    static int	pid = 0;
    int		rlen;
    static int	seq;
    register int l;
    register char *cp = m_rtmsg->m_space;
    register struct rt_msghdr	*rtm = &m_rtmsg->m_rtm;

    if (pid == 0) {
	pid = getpid();
    }

    errno = 0;
    if (cmd == RTM_DELETE)
	goto doit;

    bzero((char *)m_rtmsg, sizeof(*m_rtmsg));
    rtm->rtm_flags = 0;
    rtm->rtm_version = RTM_VERSION;

    if (cmd == RTM_GET) {
	rtm->rtm_addrs = RTA_DST;
    }

    if (sin_m) {
	bcopy((char *)sin_m, cp, sizeof(*sin_m));
	cp += sizeof(*sin_m);
	rtm->rtm_msglen = cp - (char *)m_rtmsg;
    }

doit:
    l = rtm->rtm_msglen;
    rtm->rtm_seq = ++seq;
    rtm->rtm_type = cmd;
    if ((rlen = write(s, (char *)m_rtmsg, l)) < 0) {
	if (errno != ESRCH) {
	    my_log(LOG_ERR, "rtmsg: error writing to routing socket: %d : %s",
		   cmd, strerror(errno));
	}
	return (-1);
    }
    do {
	l = read(s, (char *)m_rtmsg, sizeof(*m_rtmsg));
    } while (l > 0 && (rtm->rtm_seq != seq || rtm->rtm_pid != pid));
    if (l < 0) {
	my_log(LOG_ERR, "rtmsg: error reading from routing socket: %s",
	       strerror(errno));
    }
    return (0);
}

/* packing rule for routing socket */
#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(uint32_t) - 1))) : sizeof(uint32_t))

static void
delete_ndroute(struct sockaddr_in6 *sin_del)
{
    llocal_rtmsg_t		m_rtmsg;
    struct sockaddr_in6		sin_m;
    register struct rt_msghdr	*rtm = &m_rtmsg.m_rtm;
    struct sockaddr_in6		*sin = &sin_m;
    struct sockaddr_dl		*sdl;
    int s = inet6_routing_socket();

    if (s < 0) {
	my_log(LOG_ERR, "delete_ndroute: error opening routing socket: %s (%d)",
	       strerror(errno), errno);
	return;
    }

    my_log(LOG_DEBUG, "delete_ndroute: " IP6_FORMAT, IP6_LIST(&sin_del->sin6_addr));

    sin_m = blank_sin;
    sin->sin6_addr = sin_del->sin6_addr;
    if (IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr)) {
	*(u_int16_t *)&sin->sin6_addr.s6_addr[2] = sin_del->sin6_scope_id;
    }
    if (send_rtmsg(s, RTM_GET, &m_rtmsg, &sin_m) != 0) {
	my_log(LOG_DEBUG, "delete_ndroute: no route");
	goto bad;
    }
    sin = (struct sockaddr_in6 *)(rtm + 1);
    sdl = (struct sockaddr_dl *)(ROUNDUP(sin->sin6_len) + (char *)sin);
    if (!IN6_ARE_ADDR_EQUAL(&sin->sin6_addr, &sin_m.sin6_addr)) {
	my_log(LOG_DEBUG, "delete_ndroute: addresses do not match");
	goto bad;
    }
    else {
	if (sdl->sdl_family == AF_LINK &&
	    (rtm->rtm_flags & RTF_LLINFO) &&
	    !(rtm->rtm_flags & RTF_GATEWAY)) {
	    if (send_rtmsg(s, RTM_DELETE, &m_rtmsg, NULL) == 0) {
		struct sockaddr_in6 s6 = *sin; /* XXX: for safety */

		if (IN6_IS_ADDR_LINKLOCAL(&s6.sin6_addr)) {
		    s6.sin6_scope_id = ntohs(*(u_int16_t *)&s6.sin6_addr.s6_addr[2]);
		    *(u_int16_t *)&s6.sin6_addr.s6_addr[2] = 0;
		}

#if LLOCAL_DEBUG
	    {
		char host_buf[NI_MAXHOST];

		getnameinfo((struct sockaddr *)&s6,
			    s6.sin6_len, host_buf,
			    sizeof(host_buf), NULL, 0,
			    NI_WITHSCOPEID);
		my_log(LOG_DEBUG, "delete_ndroute: %s (%s) deleted", host, host_buf);
	    }
#endif /* LLOCAL_DEBUG */
	    }
	}
	else {
	    my_log(LOG_ERR, "delete_ndroute: cannot delete non-NDP entry");
	    goto bad;
	}
    }

bad:
    if (s > 0) {
	close(s);
    }
    return;
}

static int
linklocal_flush_ndroutes(void)
{
    int		err = 0;
    int 	mib[6];
    size_t 	needed;
    char 	*lim, *buf = NULL, *next;
    struct rt_msghdr	*rtm;
    struct sockaddr_in6	*sin;
    struct sockaddr_dl	*sdl;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_INET6;
    mib[4] = NET_RT_FLAGS;
    mib[5] = RTF_LLINFO;
    if ((sysctl(mib, 6, NULL, &needed, NULL, 0)) < 0) {
	err = errno;
	my_log(LOG_DEBUG, "linklocal_flush_ndroutes: sysctl(PF_ROUTE estimate): %s, (%d)",
	       strerror(errno), errno);
	goto done;
    }
    if (needed > 0) {
	if ((buf = malloc(needed)) == NULL) {
	    err = ENOMEM;
	    my_log(LOG_DEBUG, "linklocal_flush_ndroutes: malloc failed");
	    goto done;
	}
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
	    err = errno;
	    my_log(LOG_DEBUG, "linklocal_flush_ndroutes: sysctl(PF_ROUTE, NET_RT_FLAGS): %s, (%d)",
		   strerror(errno), errno);
	    goto done;
	}
	lim = buf + needed;
    } else
	buf = lim = NULL;

    for (next = buf; next && next < lim; next += rtm->rtm_msglen) {
	rtm = (struct rt_msghdr *)next;
	sin = (struct sockaddr_in6 *)(rtm + 1);
	sdl = (struct sockaddr_dl *)((char *)sin + ROUNDUP(sin->sin6_len));

	/*
	* Some OSes can produce a route that has the LINK flag but
	* has a non-AF_LINK gateway (e.g. fe80::xx%lo0 on FreeBSD
	* and BSD/OS, where xx is not the interface identifier on
	* lo0).
	* XXX: such routes should have the GATEWAY flag, not the
	* LINK flag.  However, there are rotten routing software
	* that advertises all routes that have the GATEWAY flag.
	* Thus, KAME kernel intentionally does not set the LINK flag.
	* What is to be fixed is not ndp, but such routing software
	* (and the kernel workaround)...
	*/
	if (sdl->sdl_family != AF_LINK)
	    continue;

	if (IN6_IS_ADDR_MULTICAST(&sin->sin6_addr))
	    continue;
	if (IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr) ||
	    IN6_IS_ADDR_MC_LINKLOCAL(&sin->sin6_addr)) {
	    if (sin->sin6_scope_id == 0)
		sin->sin6_scope_id = sdl->sdl_index;

	    /* KAME specific hack; removed the embedded id */
	    *(u_int16_t *)&sin->sin6_addr.s6_addr[2] = 0;
	}

	if (rtm->rtm_flags & RTF_WASCLONED) {
	    delete_ndroute(sin);
	}

	continue;
    }

done:
    if (buf)
	free(buf);
    return err;
}

static int
inet6_attach_interface(int s, char * ifname)
{
    int	ret = 0;

    if (siocprotoattach(s, ifname) < 0) {
	ret = errno;
	my_log(LOG_DEBUG, "siocprotoattach(%s) failed, %s (%d)",
	       ifname, strerror(errno), errno);
    }
    if (ifflags_set(s, ifname, IFF_UP) < 0) {
	my_log(LOG_DEBUG, "inet6_attach_interface %s: ifflags_set failed",
	       ifname, strerror(errno), errno);
    }

    return (ret);
}

static int
inet6_detach_interface(char * ifname)
{
    int ret = 0;
    int s = inet6_dgram_socket();

    if (s < 0) {
	ret = errno;
	goto done;
    }

    my_log(LOG_DEBUG, "inet6_detach_interface %s", ifname);

    if (linklocal_stop(s, ifname) < 0) {
	ret = errno;
	my_log(LOG_DEBUG, "linklocal_stop(%s) failed, %s (%d)",
	       ifname, strerror(errno), errno);
    }
    if (siocprotodetach(s, ifname) < 0) {
	ret = errno;
	my_log(LOG_DEBUG, "siocprotodetach(%s) failed, %s (%d)",
	       ifname, strerror(errno), errno);
    }
    if (linklocal_flush_ndroutes() != 0) {
	ret = errno;
	my_log(LOG_DEBUG, "linklocal_flush_ndroutes(%s) failed, %s (%d)",
	       ifname, strerror(errno), errno);
    }
    close(s);

 done:
    return (ret);
}

static void
llocal_cancel_pending_events(Service_t * service_p)
{
    Service_llocal_t *	llocal = (Service_llocal_t *)service_p->private;

    if (llocal == NULL)
	return;
    if (llocal->timer) {
	timer_cancel(llocal->timer);
    }
    return;
}

static void
llocal_link_timer(void * arg0, void * arg1, void * arg2)
{
    Service_t *		service_p = (Service_t *)arg0;
    interface_t *	if_p = service_interface(service_p);
    int 		s = inet6_dgram_socket();

    if (s < 0) {
	return;
    }

    my_log(LOG_DEBUG, "llocal_link_timer %s", if_name(if_p));

    /* stop linklocal */
    if (linklocal_stop(s, if_name(if_p)) != 0) {
	my_log(LOG_ERR,
	       "LINKLOCAL: error stopping linklocal on interface %s",
	       if_name(if_p));
    }
    if (linklocal_flush_ndroutes() != 0) {
	my_log(LOG_ERR, "linklocal_ndflush(%s) failed",
	       if_name(if_p));
    }

    close(s);

    return;
}

__private_extern__ ip6config_status_t
linklocal_thread(Service_t * service_p, IFEventID_t evid, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    Service_llocal_t *	llocal = (Service_llocal_t *)service_p->private;
    ip6config_status_t	status = ip6config_status_success_e;

    switch (evid) {
	case IFEventID_start_e: {
	    my_log(LOG_DEBUG, "LINKLOCAL %s: STARTING", if_name(if_p));

	    if (llocal) {
		my_log(LOG_DEBUG, "LINKLOCAL %s: re-entering start state",
		       if_name(if_p));
		status = ip6config_status_internal_error_e;
		break;
	    }

	    llocal = calloc(1, sizeof(*llocal));
	    if (llocal == NULL) {
		my_log(LOG_ERR, "LINKLOCAL %s: calloc failed",
		       if_name(if_p));
		status = ip6config_status_allocation_failed_e;
		break;
	    }

	    service_p->private = llocal;

	    llocal->timer = timer_callout_init();
	    if (llocal->timer == NULL) {
		my_log(LOG_ERR, "LINKLOCAL %s: timer_callout_init failed",
		       if_name(if_p));
		status = ip6config_status_allocation_failed_e;
		goto stop;
	    }

	    /* attach interface and start linklocal*/
	    {
		int s = inet6_dgram_socket();
		int ret;

		if (s < 0) {
		    status = ip6config_status_internal_error_e;
		    goto stop;
		}
		ret = inet6_attach_interface(s, if_name(if_p));
		if (ret && ret != EEXIST) {
		    my_log(LOG_ERR, "LINKLOCAL: inet6_attach_interface(%s) failed, %s (%d)",
			   if_name(if_p), strerror(ret), ret);
		}
		else if (service_link_status(service_p)->valid == TRUE) {
		    if (service_link_status(service_p)->active == TRUE) {
			if (linklocal_start(s, if_name(if_p)) < 0) {
			    my_log(LOG_DEBUG, "linklocal_start(%s) failed, %s (%d)",
				   if_name(if_p), strerror(errno), errno);
			}
		    }
		}
		close(s);
	    }

	    break;
	}
     stop:
	case IFEventID_stop_e: {
	    my_log(LOG_DEBUG, "LINKLOCAL %s: STOPPING", if_name(if_p));

	    if (llocal == NULL) {
		my_log(LOG_DEBUG, "LINKLOCAL %s: private data is NULL",
		       if_name(if_p));
		status = ip6config_status_internal_error_e;
		break;
	    }

	    /* clean-up resources */
	    if (llocal->timer) {
		timer_callout_free(&llocal->timer);
	    }

	    /* stop linklocal and detach interface */
	    if (inet6_detach_interface(if_name(if_p)) != 0) {
		my_log(LOG_DEBUG, "LINKLOCAL: error detaching interface %s",
		       if_name(if_p));
	    }

	    free(llocal);
	    service_p->private = NULL;
	    break;
	}
	case IFEventID_media_e: {
	    my_log(LOG_DEBUG, "LINKLOCAL %s: MEDIA CHANGE", if_name(if_p));

	    if (llocal == NULL)
		return (ip6config_status_internal_error_e);

	    if (service_link_status(service_p)->valid == TRUE) {
		if (service_link_status(service_p)->active == TRUE) {
		    /* start linklocal */
		    int s = inet6_dgram_socket();
		    llocal_cancel_pending_events(service_p);

		    if (s < 0) {
			return (ip6config_status_internal_error_e);
		    }

		    if (linklocal_start(s, if_name(if_p)) != 0) {
			my_log(LOG_ERR,
			       "LINKLOCAL: error starting linklocal on interface %s",
			       if_name(if_p));
		    }

		    close(s);
		}
		else {
		    struct timeval tv;

		    /* if link goes down and stays down long enough, unpublish */
		    llocal_cancel_pending_events(service_p);
		    tv.tv_sec = LINK_INACTIVE_WAIT_SECS;
		    tv.tv_usec = 0;
		    timer_set_relative(llocal->timer, tv,
				       (timer_func_t *)llocal_link_timer,
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
