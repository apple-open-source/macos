/*
 * Copyright (c) 2000-2021 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)in.c	8.4 (Berkeley) 1/9/95
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/socketvar.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/kern_event.h>
#include <sys/syslog.h>
#include <sys/mcache.h>
#include <sys/protosw.h>
#include <sys/file.h>

#include <kern/zalloc.h>
#include <pexpert/pexpert.h>
#include <os/log.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/kpi_protocol.h>
#include <net/dlil.h>
#if PF
#include <net/pfvar.h>
#endif /* PF */

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>

#include <net/sockaddr_utils.h>

static int inctl_associd(struct socket *, u_long, caddr_t __indexable);
static int inctl_connid(struct socket *, u_long, caddr_t __indexable);
static int inctl_conninfo(struct socket *, u_long, caddr_t __indexable);

static int inctl_autoaddr(struct ifnet *, struct ifreq *);
static int inctl_arpipll(struct ifnet *, struct ifreq *);
static int inctl_setrouter(struct ifnet *, struct ifreq *);
static int inctl_ifaddr(struct ifnet *, struct in_ifaddr *, u_long,
    struct ifreq *);
static int inctl_ifdstaddr(struct ifnet *, struct in_ifaddr *, u_long,
    struct ifreq *);
static int inctl_ifbrdaddr(struct ifnet *, struct in_ifaddr *, u_long,
    struct ifreq *);
static int inctl_ifnetmask(struct ifnet *, struct in_ifaddr *, u_long,
    struct ifreq *);

static void in_socktrim(struct sockaddr_in *);
static int in_ifinit(struct ifnet *, struct in_ifaddr *,
    struct sockaddr_in *, int);

#define IA_HASH_INIT(ia) {                                      \
	(ia)->ia_hash.tqe_next = __unsafe_forge_single(void *, ~(uintptr_t)0); \
	(ia)->ia_hash.tqe_prev = __unsafe_forge_single(void *, ~(uintptr_t)0); \
}

#define IA_IS_HASHED(ia)                                        \
	(!((ia)->ia_hash.tqe_next == __unsafe_forge_single(void *, ~(uintptr_t)0) ||  \
	(ia)->ia_hash.tqe_prev == __unsafe_forge_single(void *, ~(uintptr_t)0)))

static void in_iahash_remove(struct in_ifaddr *);
static void in_iahash_insert(struct in_ifaddr *);
static void in_iahash_insert_ptp(struct in_ifaddr *);
static struct in_ifaddr *in_ifaddr_alloc(void);
static void in_ifaddr_free(struct ifaddr *);

static int in_getassocids(struct socket *, uint32_t *, user_addr_t);
static int in_getconnids(struct socket *, sae_associd_t, uint32_t *, user_addr_t);

static int subnetsarelocal = 0;
SYSCTL_INT(_net_inet_ip, OID_AUTO, subnets_are_local,
    CTLFLAG_RW | CTLFLAG_LOCKED, &subnetsarelocal, 0, "");

/* Track whether or not the SIOCARPIPLL ioctl has been called */
u_int32_t ipv4_ll_arp_aware = 0;

/*
 * Return 1 if the address is
 * - loopback
 * - unicast or multicast link local
 * - routed via a link level gateway
 * - belongs to a directly connected (sub)net
 */
int
inaddr_local(struct in_addr in)
{
	struct rtentry *__single rt;
	struct sockaddr_in sin;
	int local = 0;

	if (ntohl(in.s_addr) == INADDR_LOOPBACK ||
	    IN_LINKLOCAL(ntohl(in.s_addr))) {
		local = 1;
	} else if (ntohl(in.s_addr) >= INADDR_UNSPEC_GROUP &&
	    ntohl(in.s_addr) <= INADDR_MAX_LOCAL_GROUP) {
		local = 1;
	} else {
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(sin);
		sin.sin_addr = in;
		rt = rtalloc1(SA(&sin), 0, 0);

		if (rt != NULL) {
			RT_LOCK_SPIN(rt);
			if (rt->rt_gateway->sa_family == AF_LINK ||
			    (rt->rt_ifp->if_flags & IFF_LOOPBACK)) {
				local = 1;
			}
			RT_UNLOCK(rt);
			rtfree(rt);
		} else {
			local = in_localaddr(in);
		}
	}
	return local;
}

/*
 * Return 1 if an internet address is for a ``local'' host
 * (one to which we have a connection).  If subnetsarelocal
 * is true, this includes other subnets of the local net,
 * otherwise, it includes the directly-connected (sub)nets.
 * The IPv4 link local prefix 169.254/16 is also included.
 */
int
in_localaddr(struct in_addr in)
{
	u_int32_t i = ntohl(in.s_addr);
	struct in_ifaddr *__single ia;

	if (IN_LINKLOCAL(i)) {
		return 1;
	}

	if (subnetsarelocal) {
		lck_rw_lock_shared(&in_ifaddr_rwlock);
		for (ia = in_ifaddrhead.tqh_first; ia != NULL;
		    ia = ia->ia_link.tqe_next) {
			IFA_LOCK(&ia->ia_ifa);
			if ((i & ia->ia_netmask) == ia->ia_net) {
				IFA_UNLOCK(&ia->ia_ifa);
				lck_rw_done(&in_ifaddr_rwlock);
				return 1;
			}
			IFA_UNLOCK(&ia->ia_ifa);
		}
		lck_rw_done(&in_ifaddr_rwlock);
	} else {
		lck_rw_lock_shared(&in_ifaddr_rwlock);
		for (ia = in_ifaddrhead.tqh_first; ia != NULL;
		    ia = ia->ia_link.tqe_next) {
			IFA_LOCK(&ia->ia_ifa);
			if ((i & ia->ia_subnetmask) == ia->ia_subnet) {
				IFA_UNLOCK(&ia->ia_ifa);
				lck_rw_done(&in_ifaddr_rwlock);
				return 1;
			}
			IFA_UNLOCK(&ia->ia_ifa);
		}
		lck_rw_done(&in_ifaddr_rwlock);
	}
	return 0;
}

/*
 * Determine whether an IP address is in a reserved set of addresses
 * that may not be forwarded, or whether datagrams to that destination
 * may be forwarded.
 */
boolean_t
in_canforward(struct in_addr in)
{
	u_int32_t i = ntohl(in.s_addr);
	u_int32_t net;

	if (IN_EXPERIMENTAL(i) || IN_MULTICAST(i)) {
		return FALSE;
	}
	if (IN_CLASSA(i)) {
		net = i & IN_CLASSA_NET;
		if (net == 0 || net == (IN_LOOPBACKNET << IN_CLASSA_NSHIFT)) {
			return FALSE;
		}
	}
	return TRUE;
}

/*
 * Trim a mask in a sockaddr
 */
static void
in_socktrim(struct sockaddr_in *ap)
{
	char *cplim = (char *)&ap->sin_addr;
	char *cp = (char *)(&ap->sin_addr + 1);

	ap->sin_len = 0;
	while (--cp >= cplim) {
		if (*cp) {
			(ap)->sin_len = (uint8_t)(cp - (char *)(ap) + 1);
			break;
		}
	}
}

static int in_interfaces;       /* number of external internet interfaces */

static int
in_domifattach(struct ifnet *ifp)
{
	int error = 0;

	VERIFY(ifp != NULL);

	if ((error = proto_plumb(PF_INET, ifp)) && error != EEXIST) {
		log(LOG_ERR, "%s: proto_plumb returned %d if=%s\n",
		    __func__, error, if_name(ifp));
		return error;
	}

	if (ifp->if_inetdata == NULL) {
		ifp->if_inetdata = zalloc_permanent_type(struct in_ifextra);
		error = 0;
	} else if (error != EEXIST) {
		/*
		 * Since the structure is never freed, we need to
		 * zero out its contents to avoid reusing stale data.
		 * A little redundant with allocation above, but it
		 * keeps the code simpler for all cases.
		 */
		IN_IFEXTRA(ifp)->netsig_len = 0;
		bzero(IN_IFEXTRA(ifp)->netsig, sizeof(IN_IFEXTRA(ifp)->netsig));
	}
	return error;
}

static __attribute__((noinline)) int
inctl_associd(struct socket *so, u_long cmd, caddr_t __indexable data)
{
	int error = 0;
	union {
		struct so_aidreq32 a32;
		struct so_aidreq64 a64;
	} u;

	VERIFY(so != NULL);

	switch (cmd) {
	case SIOCGASSOCIDS32:           /* struct so_aidreq32 */
		bcopy(data, &u.a32, sizeof(u.a32));
		error = in_getassocids(so, &u.a32.sar_cnt, u.a32.sar_aidp);
		if (error == 0) {
			bcopy(&u.a32, data, sizeof(u.a32));
		}
		break;

	case SIOCGASSOCIDS64:           /* struct so_aidreq64 */
		bcopy(data, &u.a64, sizeof(u.a64));
		error = in_getassocids(so, &u.a64.sar_cnt, (user_addr_t)u.a64.sar_aidp);
		if (error == 0) {
			bcopy(&u.a64, data, sizeof(u.a64));
		}
		break;

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

static __attribute__((noinline)) int
inctl_connid(struct socket *so, u_long cmd, caddr_t __indexable data)
{
	int error = 0;
	union {
		struct so_cidreq32 c32;
		struct so_cidreq64 c64;
	} u;

	VERIFY(so != NULL);

	switch (cmd) {
	case SIOCGCONNIDS32:            /* struct so_cidreq32 */
		bcopy(data, &u.c32, sizeof(u.c32));
		error = in_getconnids(so, u.c32.scr_aid, &u.c32.scr_cnt,
		    u.c32.scr_cidp);
		if (error == 0) {
			bcopy(&u.c32, data, sizeof(u.c32));
		}
		break;

	case SIOCGCONNIDS64:            /* struct so_cidreq64 */
		bcopy(data, &u.c64, sizeof(u.c64));
		error = in_getconnids(so, u.c64.scr_aid, &u.c64.scr_cnt,
		    (user_addr_t)u.c64.scr_cidp);
		if (error == 0) {
			bcopy(&u.c64, data, sizeof(u.c64));
		}
		break;

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

static __attribute__((noinline)) int
inctl_conninfo(struct socket *so, u_long cmd, caddr_t __indexable data)
{
	int error = 0;
	union {
		struct so_cinforeq32 ci32;
		struct so_cinforeq64 ci64;
	} u;

	VERIFY(so != NULL);

	switch (cmd) {
	case SIOCGCONNINFO32:           /* struct so_cinforeq32 */
		bcopy(data, &u.ci32, sizeof(u.ci32));
		error = in_getconninfo(so, u.ci32.scir_cid, &u.ci32.scir_flags,
		    &u.ci32.scir_ifindex, &u.ci32.scir_error, u.ci32.scir_src,
		    &u.ci32.scir_src_len, u.ci32.scir_dst, &u.ci32.scir_dst_len,
		    &u.ci32.scir_aux_type, u.ci32.scir_aux_data,
		    &u.ci32.scir_aux_len);
		if (error == 0) {
			bcopy(&u.ci32, data, sizeof(u.ci32));
		}
		break;

	case SIOCGCONNINFO64:           /* struct so_cinforeq64 */
		bcopy(data, &u.ci64, sizeof(u.ci64));
		error = in_getconninfo(so, u.ci64.scir_cid, &u.ci64.scir_flags,
		    &u.ci64.scir_ifindex, &u.ci64.scir_error, (user_addr_t)u.ci64.scir_src,
		    &u.ci64.scir_src_len, (user_addr_t)u.ci64.scir_dst, &u.ci64.scir_dst_len,
		    &u.ci64.scir_aux_type, (user_addr_t)u.ci64.scir_aux_data,
		    &u.ci64.scir_aux_len);
		if (error == 0) {
			bcopy(&u.ci64, data, sizeof(u.ci64));
		}
		break;

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

/*
 * Caller passes in the ioctl data pointer directly via "ifr", with the
 * expectation that this routine always uses bcopy() or other byte-aligned
 * memory accesses.
 */
static __attribute__((noinline)) int
inctl_autoaddr(struct ifnet *ifp, struct ifreq *ifr)
{
	int error = 0, intval;

	VERIFY(ifp != NULL);

	bcopy(&ifr->ifr_intval, &intval, sizeof(intval));

	ifnet_lock_exclusive(ifp);
	if (intval) {
		/*
		 * An interface in IPv4 router mode implies that it
		 * is configured with a static IP address and should
		 * not act as a DHCP client; prevent SIOCAUTOADDR from
		 * being set in that mode.
		 */
		if (ifp->if_eflags & IFEF_IPV4_ROUTER) {
			intval = 0;     /* be safe; clear flag if set */
			error = EBUSY;
		} else {
			if_set_eflags(ifp, IFEF_AUTOCONFIGURING);
		}
	}
	if (!intval) {
		if_clear_eflags(ifp, IFEF_AUTOCONFIGURING);
	}
	ifnet_lock_done(ifp);

	return error;
}

/*
 * Caller passes in the ioctl data pointer directly via "ifr", with the
 * expectation that this routine always uses bcopy() or other byte-aligned
 * memory accesses.
 */
static __attribute__((noinline)) int
inctl_arpipll(struct ifnet *ifp, struct ifreq *ifr)
{
	int error = 0, intval;

	VERIFY(ifp != NULL);

	bcopy(&ifr->ifr_intval, &intval, sizeof(intval));
	ipv4_ll_arp_aware = 1;

	ifnet_lock_exclusive(ifp);
	if (intval) {
		/*
		 * An interface in IPv4 router mode implies that it
		 * is configured with a static IP address and should
		 * not have to deal with IPv4 Link-Local Address;
		 * prevent SIOCARPIPLL from being set in that mode.
		 */
		if (ifp->if_eflags & IFEF_IPV4_ROUTER) {
			intval = 0;     /* be safe; clear flag if set */
			error = EBUSY;
		} else {
			if_set_eflags(ifp, IFEF_ARPLL);
		}
	}
	if (!intval) {
		if_clear_eflags(ifp, IFEF_ARPLL);
	}
	ifnet_lock_done(ifp);

	return error;
}

/*
 * Handle SIOCSETROUTERMODE to set or clear the IPv4 router mode flag on
 * the interface.  When in this mode, IPv4 Link-Local Address support is
 * disabled in ARP, and DHCP client support is disabled in IP input; turning
 * any of them on would cause an error to be returned.  Entering or exiting
 * this mode will result in the removal of IPv4 addresses currently configured
 * on the interface.
 *
 * Caller passes in the ioctl data pointer directly via "ifr", with the
 * expectation that this routine always uses bcopy() or other byte-aligned
 * memory accesses.
 */
static __attribute__((noinline)) int
inctl_setrouter(struct ifnet *ifp, struct ifreq *ifr)
{
	int error = 0, intval;

	VERIFY(ifp != NULL);

	/* Router mode isn't valid for loopback */
	if (ifp->if_flags & IFF_LOOPBACK) {
		return ENODEV;
	}

	bcopy(&ifr->ifr_intval, &intval, sizeof(intval));
	switch (intval) {
	case 0:
	case 1:
		break;
	default:
		return EINVAL;
	}
	ifnet_lock_exclusive(ifp);
	if (intval != 0) {
		if_set_eflags(ifp, IFEF_IPV4_ROUTER);
		if_clear_eflags(ifp, (IFEF_ARPLL | IFEF_AUTOCONFIGURING));
	} else {
		if_clear_eflags(ifp, IFEF_IPV4_ROUTER);
	}
	ifnet_lock_done(ifp);

	/* purge all IPv4 addresses configured on this interface */
	in_purgeaddrs(ifp);

	return error;
}

/*
 * Caller passes in the ioctl data pointer directly via "ifr", with the
 * expectation that this routine always uses bcopy() or other byte-aligned
 * memory accesses.
 */
static __attribute__((noinline)) int
inctl_ifaddr(struct ifnet *ifp, struct in_ifaddr *ia, u_long cmd,
    struct ifreq *ifr)
{
	struct kev_in_data in_event_data;
	struct kev_msg ev_msg;
	struct sockaddr_in addr;
	struct ifaddr *__single ifa;
	int error = 0;

	VERIFY(ifp != NULL);

	bzero(&in_event_data, sizeof(struct kev_in_data));
	bzero(&ev_msg, sizeof(struct kev_msg));

	switch (cmd) {
	case SIOCGIFADDR:               /* struct ifreq */
		if (ia == NULL) {
			error = EADDRNOTAVAIL;
			break;
		}
		IFA_LOCK(&ia->ia_ifa);
		SOCKADDR_COPY(&ia->ia_addr, &ifr->ifr_addr, sizeof(addr));
		IFA_UNLOCK(&ia->ia_ifa);
		break;

	case SIOCSIFADDR:               /* struct ifreq */
		VERIFY(ia != NULL);
		SOCKADDR_COPY(&ifr->ifr_addr, &addr, sizeof(addr));
		/*
		 * If this is a new address, the reference count for the
		 * hash table has been taken at creation time above.
		 */
		error = in_ifinit(ifp, ia, &addr, 1);
		if (error == 0) {
			(void) ifnet_notify_address(ifp, AF_INET);
		}
		break;

	case SIOCAIFADDR: {             /* struct {if,in_}aliasreq */
		struct in_aliasreq *__single ifra = (struct in_aliasreq *)ifr;
		struct sockaddr_in broadaddr, mask;
		int hostIsNew, maskIsNew;

		VERIFY(ia != NULL);
		SOCKADDR_COPY(&ifra->ifra_addr, &addr, sizeof(addr));
		SOCKADDR_COPY(&ifra->ifra_broadaddr, &broadaddr, sizeof(broadaddr));
		SOCKADDR_COPY(&ifra->ifra_mask, &mask, sizeof(mask));

		maskIsNew = 0;
		hostIsNew = 1;
		error = 0;

		IFA_LOCK(&ia->ia_ifa);
		if (ia->ia_addr.sin_family == AF_INET) {
			if (addr.sin_len == 0) {
				addr = ia->ia_addr;
				hostIsNew = 0;
			} else if (addr.sin_addr.s_addr ==
			    ia->ia_addr.sin_addr.s_addr) {
				hostIsNew = 0;
			}
		}
		if (mask.sin_len != 0) {
			IFA_UNLOCK(&ia->ia_ifa);
			in_ifscrub(ifp, ia, 0);
			IFA_LOCK(&ia->ia_ifa);
			ia->ia_sockmask.sin_len = sizeof(struct sockaddr_in);
			ia->ia_sockmask.sin_family = AF_INET;
			ia->ia_sockmask.sin_port = 0;
			ia->ia_sockmask.sin_addr = mask.sin_addr;
			bzero(&ia->ia_sockmask.sin_zero, sizeof(ia->ia_dstaddr.sin_zero));
			ia->ia_subnetmask =
			    ntohl(ia->ia_sockmask.sin_addr.s_addr);
			maskIsNew = 1;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    (broadaddr.sin_family == AF_INET)) {
			IFA_UNLOCK(&ia->ia_ifa);
			in_ifscrub(ifp, ia, 0);
			IFA_LOCK(&ia->ia_ifa);
			ia->ia_dstaddr.sin_family = AF_INET;
			ia->ia_dstaddr.sin_len = sizeof(struct sockaddr_in);
			ia->ia_dstaddr.sin_port = 0;
			ia->ia_dstaddr.sin_addr = broadaddr.sin_addr;
			bzero(&ia->ia_dstaddr.sin_zero, sizeof(ia->ia_dstaddr.sin_zero));
			maskIsNew  = 1; /* We lie; but the effect's the same */
		}
		if (addr.sin_family == AF_INET && (hostIsNew || maskIsNew)) {
			IFA_UNLOCK(&ia->ia_ifa);
			error = in_ifinit(ifp, ia, &addr, 0);
		} else {
			IFA_UNLOCK(&ia->ia_ifa);
		}
		if (error == 0) {
			(void) ifnet_notify_address(ifp, AF_INET);
		}
		IFA_LOCK(&ia->ia_ifa);
		if ((ifp->if_flags & IFF_BROADCAST) &&
		    (broadaddr.sin_family == AF_INET)) {
			ia->ia_broadaddr.sin_family = AF_INET;
			ia->ia_broadaddr.sin_len = sizeof(struct sockaddr_in);
			ia->ia_broadaddr.sin_port = 0;
			ia->ia_broadaddr.sin_addr = broadaddr.sin_addr;
			bzero(&ia->ia_broadaddr.sin_zero, sizeof(ia->ia_broadaddr.sin_zero));
		}

		/*
		 * Report event.
		 */
		if ((error == 0) || (error == EEXIST)) {
			ev_msg.vendor_code      = KEV_VENDOR_APPLE;
			ev_msg.kev_class        = KEV_NETWORK_CLASS;
			ev_msg.kev_subclass     = KEV_INET_SUBCLASS;

			if (hostIsNew) {
				ev_msg.event_code = KEV_INET_NEW_ADDR;
			} else {
				ev_msg.event_code = KEV_INET_CHANGED_ADDR;
			}

			if (ia->ia_ifa.ifa_dstaddr) {
				in_event_data.ia_dstaddr = SIN(ia->ia_ifa.ifa_dstaddr)->sin_addr;
			} else {
				in_event_data.ia_dstaddr.s_addr = INADDR_ANY;
			}
			in_event_data.ia_addr           = ia->ia_addr.sin_addr;
			in_event_data.ia_net            = ia->ia_net;
			in_event_data.ia_netmask        = ia->ia_netmask;
			in_event_data.ia_subnet         = ia->ia_subnet;
			in_event_data.ia_subnetmask     = ia->ia_subnetmask;
			in_event_data.ia_netbroadcast   = ia->ia_netbroadcast;
			IFA_UNLOCK(&ia->ia_ifa);
			(void) strlcpy(&in_event_data.link_data.if_name[0],
			    ifp->if_name, IFNAMSIZ);
			in_event_data.link_data.if_family = ifp->if_family;
			in_event_data.link_data.if_unit = ifp->if_unit;

			ev_msg.dv[0].data_ptr    = &in_event_data;
			ev_msg.dv[0].data_length = sizeof(struct kev_in_data);
			ev_msg.dv[1].data_length = 0;

			dlil_post_complete_msg(ifp, &ev_msg);
		} else {
			IFA_UNLOCK(&ia->ia_ifa);
		}
		break;
	}

	case SIOCDIFADDR:               /* struct ifreq */
		VERIFY(ia != NULL);
		error = ifnet_ioctl(ifp, PF_INET, SIOCDIFADDR, ia);
		if (error == EOPNOTSUPP) {
			error = 0;
		}
		if (error != 0) {
			break;
		}

		/* Fill out the kernel event information */
		ev_msg.vendor_code      = KEV_VENDOR_APPLE;
		ev_msg.kev_class        = KEV_NETWORK_CLASS;
		ev_msg.kev_subclass     = KEV_INET_SUBCLASS;

		ev_msg.event_code       = KEV_INET_ADDR_DELETED;

		IFA_LOCK(&ia->ia_ifa);
		if (ia->ia_ifa.ifa_dstaddr) {
			in_event_data.ia_dstaddr = SIN(ia->ia_ifa.ifa_dstaddr)->sin_addr;
		} else {
			in_event_data.ia_dstaddr.s_addr = INADDR_ANY;
		}
		in_event_data.ia_addr           = ia->ia_addr.sin_addr;
		in_event_data.ia_net            = ia->ia_net;
		in_event_data.ia_netmask        = ia->ia_netmask;
		in_event_data.ia_subnet         = ia->ia_subnet;
		in_event_data.ia_subnetmask     = ia->ia_subnetmask;
		in_event_data.ia_netbroadcast   = ia->ia_netbroadcast;
		IFA_UNLOCK(&ia->ia_ifa);
		(void) strlcpy(&in_event_data.link_data.if_name[0],
		    ifp->if_name, IFNAMSIZ);
		in_event_data.link_data.if_family = ifp->if_family;
		in_event_data.link_data.if_unit  = (u_int32_t)ifp->if_unit;

		ev_msg.dv[0].data_ptr    = &in_event_data;
		ev_msg.dv[0].data_length = sizeof(struct kev_in_data);
		ev_msg.dv[1].data_length = 0;

		ifa = &ia->ia_ifa;
		lck_rw_lock_exclusive(&in_ifaddr_rwlock);
		/* Release ia_link reference */
		ifa_remref(ifa);
		TAILQ_REMOVE(&in_ifaddrhead, ia, ia_link);
		IFA_LOCK(ifa);
		if (IA_IS_HASHED(ia)) {
			in_iahash_remove(ia);
		}
		IFA_UNLOCK(ifa);
		lck_rw_done(&in_ifaddr_rwlock);

		/*
		 * in_ifscrub kills the interface route.
		 */
		in_ifscrub(ifp, ia, 0);
		ifnet_lock_exclusive(ifp);
		IFA_LOCK(ifa);
		/* if_detach_ifa() releases ifa_link reference */
		if_detach_ifa(ifp, ifa);
		/* Our reference to this address is dropped at the bottom */
		IFA_UNLOCK(ifa);

		/* invalidate route caches */
		routegenid_inet_update();

		/*
		 * If the interface supports multicast, and no address is left,
		 * remove the "all hosts" multicast group from that interface.
		 */
		if ((ifp->if_flags & IFF_MULTICAST) ||
		    ifp->if_allhostsinm != NULL) {
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				IFA_LOCK(ifa);
				if (ifa->ifa_addr->sa_family == AF_INET) {
					IFA_UNLOCK(ifa);
					break;
				}
				IFA_UNLOCK(ifa);
			}
			ifnet_lock_done(ifp);

			lck_mtx_lock(&ifp->if_addrconfig_lock);
			if (ifa == NULL && ifp->if_allhostsinm != NULL) {
				struct in_multi *__single inm = ifp->if_allhostsinm;
				ifp->if_allhostsinm = NULL;

				in_delmulti(inm);
				/* release the reference for allhostsinm */
				INM_REMREF(inm);
			}
			lck_mtx_unlock(&ifp->if_addrconfig_lock);
		} else {
			ifnet_lock_done(ifp);
		}

		/* Post the kernel event */
		dlil_post_complete_msg(ifp, &ev_msg);

		/*
		 * See if there is any IPV4 address left and if so,
		 * reconfigure KDP to use current primary address.
		 */
		ifa = ifa_ifpgetprimary(ifp, AF_INET);
		if (ifa != NULL) {
			/*
			 * NOTE: SIOCSIFADDR is defined with struct ifreq
			 * as parameter, but here we are sending it down
			 * to the interface with a pointer to struct ifaddr,
			 * for legacy reasons.
			 */
			error = ifnet_ioctl(ifp, PF_INET, SIOCSIFADDR, ifa);
			if (error == EOPNOTSUPP) {
				error = 0;
			}

			/* Release reference from ifa_ifpgetprimary() */
			ifa_remref(ifa);
		}
		(void) ifnet_notify_address(ifp, AF_INET);
		break;

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

/*
 * Caller passes in the ioctl data pointer directly via "ifr", with the
 * expectation that this routine always uses bcopy() or other byte-aligned
 * memory accesses.
 */
static __attribute__((noinline)) int
inctl_ifdstaddr(struct ifnet *ifp, struct in_ifaddr *ia, u_long cmd,
    struct ifreq *ifr)
{
	struct kev_in_data in_event_data;
	struct kev_msg ev_msg;
	struct sockaddr_in dstaddr;
	int error = 0;

	VERIFY(ifp != NULL);

	if (!(ifp->if_flags & IFF_POINTOPOINT)) {
		return EINVAL;
	}

	bzero(&in_event_data, sizeof(struct kev_in_data));
	bzero(&ev_msg, sizeof(struct kev_msg));

	switch (cmd) {
	case SIOCGIFDSTADDR:            /* struct ifreq */
		if (ia == NULL) {
			error = EADDRNOTAVAIL;
			break;
		}
		IFA_LOCK(&ia->ia_ifa);
		SOCKADDR_COPY(&ia->ia_dstaddr, &ifr->ifr_dstaddr, sizeof(dstaddr));
		IFA_UNLOCK(&ia->ia_ifa);
		break;

	case SIOCSIFDSTADDR:            /* struct ifreq */
		VERIFY(ia != NULL);
		IFA_LOCK(&ia->ia_ifa);
		dstaddr = ia->ia_dstaddr;

		ia->ia_dstaddr.sin_family = AF_INET;
		ia->ia_dstaddr.sin_len = sizeof(struct sockaddr_in);
		ia->ia_dstaddr.sin_port = 0;
		bcopy(&SIN(&ifr->ifr_dstaddr)->sin_addr,
		    &ia->ia_dstaddr.sin_addr, sizeof(ia->ia_dstaddr.sin_addr));
		bzero(&ia->ia_dstaddr.sin_zero, sizeof(ia->ia_dstaddr.sin_zero));

		IFA_UNLOCK(&ia->ia_ifa);
		/*
		 * NOTE: SIOCSIFDSTADDR is defined with struct ifreq
		 * as parameter, but here we are sending it down
		 * to the interface with a pointer to struct ifaddr,
		 * for legacy reasons.
		 */
		error = ifnet_ioctl(ifp, PF_INET, SIOCSIFDSTADDR, ia);
		IFA_LOCK(&ia->ia_ifa);
		if (error == EOPNOTSUPP) {
			error = 0;
		}
		if (error != 0) {
			ia->ia_dstaddr = dstaddr;
			IFA_UNLOCK(&ia->ia_ifa);
			break;
		}
		IFA_LOCK_ASSERT_HELD(&ia->ia_ifa);

		ev_msg.vendor_code      = KEV_VENDOR_APPLE;
		ev_msg.kev_class        = KEV_NETWORK_CLASS;
		ev_msg.kev_subclass     = KEV_INET_SUBCLASS;

		ev_msg.event_code       = KEV_INET_SIFDSTADDR;

		if (ia->ia_ifa.ifa_dstaddr) {
			in_event_data.ia_dstaddr = SIN(ia->ia_ifa.ifa_dstaddr)->sin_addr;
		} else {
			in_event_data.ia_dstaddr.s_addr = INADDR_ANY;
		}

		in_event_data.ia_addr           = ia->ia_addr.sin_addr;
		in_event_data.ia_net            = ia->ia_net;
		in_event_data.ia_netmask        = ia->ia_netmask;
		in_event_data.ia_subnet         = ia->ia_subnet;
		in_event_data.ia_subnetmask     = ia->ia_subnetmask;
		in_event_data.ia_netbroadcast   = ia->ia_netbroadcast;
		IFA_UNLOCK(&ia->ia_ifa);
		(void) strlcpy(&in_event_data.link_data.if_name[0],
		    ifp->if_name, IFNAMSIZ);
		in_event_data.link_data.if_family = ifp->if_family;
		in_event_data.link_data.if_unit  = (u_int32_t)ifp->if_unit;

		ev_msg.dv[0].data_ptr    = &in_event_data;
		ev_msg.dv[0].data_length = sizeof(struct kev_in_data);
		ev_msg.dv[1].data_length = 0;

		dlil_post_complete_msg(ifp, &ev_msg);

		lck_mtx_lock(rnh_lock);
		IFA_LOCK(&ia->ia_ifa);
		if (ia->ia_flags & IFA_ROUTE) {
			ia->ia_ifa.ifa_dstaddr = SA(&dstaddr);
			IFA_UNLOCK(&ia->ia_ifa);
			rtinit_locked(&(ia->ia_ifa), RTM_DELETE, RTF_HOST);
			IFA_LOCK(&ia->ia_ifa);
			ia->ia_ifa.ifa_dstaddr =
			    SA(&ia->ia_dstaddr);
			IFA_UNLOCK(&ia->ia_ifa);
			rtinit_locked(&(ia->ia_ifa), RTM_ADD,
			    RTF_HOST | RTF_UP);
		} else {
			IFA_UNLOCK(&ia->ia_ifa);
		}
		lck_mtx_unlock(rnh_lock);
		break;



	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

/*
 * Caller passes in the ioctl data pointer directly via "ifr", with the
 * expectation that this routine always uses bcopy() or other byte-aligned
 * memory accesses.
 */
static __attribute__((noinline)) int
inctl_ifbrdaddr(struct ifnet *ifp, struct in_ifaddr *ia, u_long cmd,
    struct ifreq *ifr)
{
	struct kev_in_data in_event_data;
	struct kev_msg ev_msg;
	int error = 0;

	VERIFY(ifp != NULL);

	if (ia == NULL) {
		return EADDRNOTAVAIL;
	}

	if (!(ifp->if_flags & IFF_BROADCAST)) {
		return EINVAL;
	}

	bzero(&in_event_data, sizeof(struct kev_in_data));
	bzero(&ev_msg, sizeof(struct kev_msg));

	switch (cmd) {
	case SIOCGIFBRDADDR:            /* struct ifreq */
		IFA_LOCK(&ia->ia_ifa);
		SOCKADDR_COPY(&ia->ia_broadaddr, &ifr->ifr_broadaddr,
		    sizeof(struct sockaddr_in));
		IFA_UNLOCK(&ia->ia_ifa);
		break;

	case SIOCSIFBRDADDR:            /* struct ifreq */
		IFA_LOCK(&ia->ia_ifa);

		ia->ia_broadaddr.sin_family = AF_INET;
		ia->ia_broadaddr.sin_len = sizeof(struct sockaddr_in);
		ia->ia_broadaddr.sin_port = 0;
		bcopy(&SIN(&ifr->ifr_broadaddr)->sin_addr,
		    &ia->ia_broadaddr.sin_addr, sizeof(ia->ia_broadaddr.sin_addr));
		bzero(&ia->ia_broadaddr.sin_zero, sizeof(ia->ia_broadaddr.sin_zero));

		ev_msg.vendor_code      = KEV_VENDOR_APPLE;
		ev_msg.kev_class        = KEV_NETWORK_CLASS;
		ev_msg.kev_subclass     = KEV_INET_SUBCLASS;

		ev_msg.event_code = KEV_INET_SIFBRDADDR;

		if (ia->ia_ifa.ifa_dstaddr) {
			in_event_data.ia_dstaddr = SIN(ia->ia_ifa.ifa_dstaddr)->sin_addr;
		} else {
			in_event_data.ia_dstaddr.s_addr = INADDR_ANY;
		}
		in_event_data.ia_addr           = ia->ia_addr.sin_addr;
		in_event_data.ia_net            = ia->ia_net;
		in_event_data.ia_netmask        = ia->ia_netmask;
		in_event_data.ia_subnet         = ia->ia_subnet;
		in_event_data.ia_subnetmask     = ia->ia_subnetmask;
		in_event_data.ia_netbroadcast   = ia->ia_netbroadcast;
		IFA_UNLOCK(&ia->ia_ifa);
		(void) strlcpy(&in_event_data.link_data.if_name[0],
		    ifp->if_name, IFNAMSIZ);
		in_event_data.link_data.if_family = ifp->if_family;
		in_event_data.link_data.if_unit  = (u_int32_t)ifp->if_unit;

		ev_msg.dv[0].data_ptr    = &in_event_data;
		ev_msg.dv[0].data_length = sizeof(struct kev_in_data);
		ev_msg.dv[1].data_length = 0;

		dlil_post_complete_msg(ifp, &ev_msg);
		break;

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

/*
 * Caller passes in the ioctl data pointer directly via "ifr", with the
 * expectation that this routine always uses bcopy() or other byte-aligned
 * memory accesses.
 */
static __attribute__((noinline)) int
inctl_ifnetmask(struct ifnet *ifp, struct in_ifaddr *ia, u_long cmd,
    struct ifreq *ifr)
{
	struct kev_in_data in_event_data;
	struct kev_msg ev_msg;
	struct sockaddr_in mask;
	int error = 0;

	VERIFY(ifp != NULL);

	bzero(&in_event_data, sizeof(struct kev_in_data));
	bzero(&ev_msg, sizeof(struct kev_msg));

	switch (cmd) {
	case SIOCGIFNETMASK:            /* struct ifreq */
		if (ia == NULL) {
			error = EADDRNOTAVAIL;
			break;
		}
		IFA_LOCK(&ia->ia_ifa);
		SOCKADDR_COPY(&ia->ia_sockmask, &ifr->ifr_addr, sizeof(mask));
		IFA_UNLOCK(&ia->ia_ifa);
		break;

	case SIOCSIFNETMASK: {          /* struct ifreq */
		in_addr_t i;

		SOCKADDR_COPY(&ifr->ifr_addr, &mask, sizeof(mask));
		i = mask.sin_addr.s_addr;

		VERIFY(ia != NULL);
		IFA_LOCK(&ia->ia_ifa);
		ia->ia_subnetmask = ntohl(ia->ia_sockmask.sin_addr.s_addr = i);
		ev_msg.vendor_code      = KEV_VENDOR_APPLE;
		ev_msg.kev_class        = KEV_NETWORK_CLASS;
		ev_msg.kev_subclass     = KEV_INET_SUBCLASS;

		ev_msg.event_code = KEV_INET_SIFNETMASK;

		if (ia->ia_ifa.ifa_dstaddr) {
			in_event_data.ia_dstaddr = SIN(ia->ia_ifa.ifa_dstaddr)->sin_addr;
		} else {
			in_event_data.ia_dstaddr.s_addr = INADDR_ANY;
		}
		in_event_data.ia_addr           = ia->ia_addr.sin_addr;
		in_event_data.ia_net            = ia->ia_net;
		in_event_data.ia_netmask        = ia->ia_netmask;
		in_event_data.ia_subnet         = ia->ia_subnet;
		in_event_data.ia_subnetmask     = ia->ia_subnetmask;
		in_event_data.ia_netbroadcast   = ia->ia_netbroadcast;
		IFA_UNLOCK(&ia->ia_ifa);
		(void) strlcpy(&in_event_data.link_data.if_name[0],
		    ifp->if_name, IFNAMSIZ);
		in_event_data.link_data.if_family = ifp->if_family;
		in_event_data.link_data.if_unit  = (u_int32_t)ifp->if_unit;

		ev_msg.dv[0].data_ptr    = &in_event_data;
		ev_msg.dv[0].data_length = sizeof(struct kev_in_data);
		ev_msg.dv[1].data_length = 0;

		dlil_post_complete_msg(ifp, &ev_msg);
		break;
	}

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

/*
 * Generic INET control operations (ioctl's).
 *
 * ifp is NULL if not an interface-specific ioctl.
 *
 * Most of the routines called to handle the ioctls would end up being
 * tail-call optimized, which unfortunately causes this routine to
 * consume too much stack space; this is the reason for the "noinline"
 * attribute used on those routines.
 *
 * If called directly from within the networking stack (as opposed to via
 * pru_control), the socket parameter may be NULL.
 */
int
in_control(struct socket *so, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data,
    struct ifnet *ifp, struct proc *p)
{
	struct ifreq *__single ifr = NULL;
	struct sockaddr_in addr, dstaddr;
	struct sockaddr_in sin;
	struct sockaddr_in *__single sa = NULL;
	boolean_t privileged = (proc_suser(p) == 0);
	boolean_t so_unlocked = FALSE;
	struct in_ifaddr *__single ia = NULL;
	struct ifaddr *__single ifa;
	int error = 0;
	int intval;

	/* In case it's NULL, make sure it came from the kernel */
	VERIFY(so != NULL || p == kernproc);

	/*
	 * ioctls which don't require ifp, but require socket.
	 */
	switch (cmd) {
	case SIOCGASSOCIDS32:           /* struct so_aidreq32 */
	case SIOCGASSOCIDS64:           /* struct so_aidreq64 */
		return inctl_associd(so, cmd, data);
	/* NOTREACHED */

	case SIOCGCONNIDS32:            /* struct so_cidreq32 */
	case SIOCGCONNIDS64:            /* struct so_cidreq64 */
		return inctl_connid(so, cmd, data);
	/* NOTREACHED */

	case SIOCGCONNINFO32:           /* struct so_cinforeq32 */
	case SIOCGCONNINFO64:           /* struct so_cinforeq64 */
		return inctl_conninfo(so, cmd, data);
		/* NOTREACHED */
	}

	/*
	 * The rest of ioctls require ifp; reject if we don't have one;
	 * return ENXIO to be consistent with ifioctl().
	 */
	if (ifp == NULL) {
		return ENXIO;
	}

	/*
	 * ioctls which require ifp but not interface address.
	 */
	switch (cmd) {
	case SIOCAUTOADDR:              /* struct ifreq */
		if (!privileged) {
			return EPERM;
		}
		ifr = (struct ifreq *)(void *)data;
		return inctl_autoaddr(ifp, ifr);
	/* NOTREACHED */

	case SIOCARPIPLL:               /* struct ifreq */
		if (!privileged) {
			return EPERM;
		}
		ifr = (struct ifreq *)(void *)data;
		return inctl_arpipll(ifp, ifr);
	/* NOTREACHED */

	case SIOCGETROUTERMODE:         /* struct ifreq */
		ifr = (struct ifreq *)(void *)data;
		intval = (ifp->if_eflags & IFEF_IPV4_ROUTER) != 0 ? 1 : 0;
		bcopy(&intval, &ifr->ifr_intval, sizeof(intval));
		return 0;
	/* NOTREACHED */

	case SIOCSETROUTERMODE:         /* struct ifreq */
		if (!privileged) {
			return EPERM;
		}
		ifr = (struct ifreq *)(void *)data;
		return inctl_setrouter(ifp, ifr);
	/* NOTREACHED */

	case SIOCPROTOATTACH:           /* struct ifreq */
		if (!privileged) {
			return EPERM;
		}
		return in_domifattach(ifp);
	/* NOTREACHED */

	case SIOCPROTODETACH:           /* struct ifreq */
		if (!privileged) {
			return EPERM;
		}

		/*
		 * If an IPv4 address is still present, refuse to detach.
		 */
		ifnet_lock_shared(ifp);
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			IFA_LOCK(ifa);
			if (ifa->ifa_addr->sa_family == AF_INET) {
				IFA_UNLOCK(ifa);
				break;
			}
			IFA_UNLOCK(ifa);
		}
		ifnet_lock_done(ifp);
		return (ifa == NULL) ? proto_unplumb(PF_INET, ifp) : EBUSY;
		/* NOTREACHED */
	}

	/*
	 * ioctls which require interface address; obtain sockaddr_in.
	 */
	switch (cmd) {
	case SIOCAIFADDR:               /* struct {if,in_}aliasreq */
		if (!privileged) {
			return EPERM;
		}
		SOCKADDR_COPY(&((struct in_aliasreq *)(void *)data)->ifra_addr,
		    &sin, sizeof(sin));
		sa = &sin;
		break;

	case SIOCDIFADDR:               /* struct ifreq */
	case SIOCSIFADDR:               /* struct ifreq */
	case SIOCSIFDSTADDR:            /* struct ifreq */
	case SIOCSIFNETMASK:            /* struct ifreq */
	case SIOCSIFBRDADDR:            /* struct ifreq */
		if (!privileged) {
			return EPERM;
		}
		OS_FALLTHROUGH;
	case SIOCGIFADDR:               /* struct ifreq */
	case SIOCGIFDSTADDR:            /* struct ifreq */
	case SIOCGIFNETMASK:            /* struct ifreq */
	case SIOCGIFBRDADDR:            /* struct ifreq */
		ifr = (struct ifreq *)(void *)data;
		SOCKADDR_COPY(&ifr->ifr_addr, &sin, sizeof(sin));
		sa = &sin;
		break;
	}

	/*
	 * Find address for this interface, if it exists.
	 *
	 * If an alias address was specified, find that one instead of
	 * the first one on the interface, if possible.
	 */
	VERIFY(ia == NULL);
	if (sa != NULL) {
		struct in_ifaddr *iap;

		/*
		 * Any failures from this point on must take into account
		 * a non-NULL "ia" with an outstanding reference count, and
		 * therefore requires ifa_remref.  Jump to "done" label
		 * instead of calling return if "ia" is valid.
		 */
		lck_rw_lock_shared(&in_ifaddr_rwlock);
		TAILQ_FOREACH(iap, INADDR_HASH(sa->sin_addr.s_addr), ia_hash) {
			IFA_LOCK(&iap->ia_ifa);
			if (iap->ia_ifp == ifp &&
			    iap->ia_addr.sin_addr.s_addr ==
			    sa->sin_addr.s_addr) {
				ia = iap;
				ifa_addref(&iap->ia_ifa);
				IFA_UNLOCK(&iap->ia_ifa);
				break;
			}
			IFA_UNLOCK(&iap->ia_ifa);
		}
		lck_rw_done(&in_ifaddr_rwlock);

		if (ia == NULL) {
			ifnet_lock_shared(ifp);
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				iap = ifatoia(ifa);
				IFA_LOCK(&iap->ia_ifa);
				if (iap->ia_addr.sin_family == AF_INET) {
					ia = iap;
					ifa_addref(&iap->ia_ifa);
					IFA_UNLOCK(&iap->ia_ifa);
					break;
				}
				IFA_UNLOCK(&iap->ia_ifa);
			}
			ifnet_lock_done(ifp);
		}
	}

	/*
	 * Unlock the socket since ifnet_ioctl() may be invoked by
	 * one of the ioctl handlers below.  Socket will be re-locked
	 * prior to returning.
	 */
	if (so != NULL) {
		socket_unlock(so, 0);
		so_unlocked = TRUE;
	}

	switch (cmd) {
	case SIOCAIFADDR:               /* struct {if,in_}aliasreq */
	case SIOCDIFADDR:               /* struct ifreq */
		if (cmd == SIOCAIFADDR) {
			SOCKADDR_COPY(&((struct in_aliasreq *)(void *)data)->
			    ifra_addr, &addr, sizeof(addr));
			SOCKADDR_COPY(&((struct in_aliasreq *)(void *)data)->
			    ifra_dstaddr, &dstaddr, sizeof(dstaddr));
		} else {
			VERIFY(cmd == SIOCDIFADDR);
			SOCKADDR_COPY(&((struct ifreq *)(void *)data)->ifr_addr,
			    &addr, sizeof(addr));
			SOCKADDR_ZERO(&dstaddr, sizeof(dstaddr));
		}

		if (addr.sin_family == AF_INET) {
			struct in_ifaddr *__single oia;

			lck_rw_lock_shared(&in_ifaddr_rwlock);
			for (oia = ia; ia; ia = ia->ia_link.tqe_next) {
				IFA_LOCK(&ia->ia_ifa);
				if (ia->ia_ifp == ifp &&
				    ia->ia_addr.sin_addr.s_addr ==
				    addr.sin_addr.s_addr) {
					ifa_addref(&ia->ia_ifa);
					IFA_UNLOCK(&ia->ia_ifa);
					break;
				}
				IFA_UNLOCK(&ia->ia_ifa);
			}
			lck_rw_done(&in_ifaddr_rwlock);
			if (oia != NULL) {
				ifa_remref(&oia->ia_ifa);
			}
			if ((ifp->if_flags & IFF_POINTOPOINT) &&
			    (cmd == SIOCAIFADDR) &&
			    (dstaddr.sin_addr.s_addr == INADDR_ANY)) {
				error = EDESTADDRREQ;
				goto done;
			}
		} else if (cmd == SIOCAIFADDR) {
			error = EINVAL;
			goto done;
		}
		if (cmd == SIOCDIFADDR) {
			if (ia == NULL) {
				error = EADDRNOTAVAIL;
				goto done;
			}

			IFA_LOCK(&ia->ia_ifa);
			/*
			 * Avoid the race condition seen when two
			 * threads process SIOCDIFADDR command
			 * at the same time.
			 */
			while (ia->ia_ifa.ifa_debug & IFD_DETACHING) {
				os_log(OS_LOG_DEFAULT,
				    "Another thread is already attempting to "
				    "delete IPv4 address: %s on interface %s. "
				    "Go to sleep and check again after the operation is done",
				    inet_ntoa(sa->sin_addr), ia->ia_ifp->if_xname);
				ia->ia_ifa.ifa_del_waiters++;
				(void) msleep(ia->ia_ifa.ifa_del_wc, &ia->ia_ifa.ifa_lock, (PZERO - 1),
				    __func__, NULL);
				IFA_LOCK_ASSERT_HELD(&ia->ia_ifa);
			}

			if ((ia->ia_ifa.ifa_debug & IFD_ATTACHED) == 0) {
				error = EADDRNOTAVAIL;
				IFA_UNLOCK(&ia->ia_ifa);
				goto done;
			}

			ia->ia_ifa.ifa_debug |= IFD_DETACHING;
			IFA_UNLOCK(&ia->ia_ifa);
		}

		OS_FALLTHROUGH;
	case SIOCSIFADDR:               /* struct ifreq */
	case SIOCSIFDSTADDR:            /* struct ifreq */
	case SIOCSIFNETMASK:            /* struct ifreq */
		if (cmd == SIOCAIFADDR) {
			/* fell thru from above; just repeat it */
			SOCKADDR_COPY(&((struct in_aliasreq *)(void *)data)->
			    ifra_addr, &addr, sizeof(addr));
		} else {
			VERIFY(cmd == SIOCDIFADDR || cmd == SIOCSIFADDR ||
			    cmd == SIOCSIFNETMASK || cmd == SIOCSIFDSTADDR);
			SOCKADDR_COPY(&((struct ifreq *)(void *)data)->ifr_addr,
			    &addr, sizeof(addr));
		}

		if (addr.sin_family != AF_INET && cmd == SIOCSIFADDR) {
			error = EINVAL;
			goto done;
		}

		if ((cmd == SIOCAIFADDR || cmd == SIOCSIFADDR) &&
		    (IN_MULTICAST(ntohl(addr.sin_addr.s_addr)) ||
		    addr.sin_addr.s_addr == INADDR_BROADCAST ||
		    addr.sin_addr.s_addr == INADDR_ANY)) {
			error = EINVAL;
			goto done;
		}

		if (ia == NULL) {
			ia = in_ifaddr_alloc();
			if (ia == NULL) {
				error = ENOBUFS;
				goto done;
			}
			ifnet_lock_exclusive(ifp);
			ifa = &ia->ia_ifa;
			IFA_LOCK(ifa);
			IA_HASH_INIT(ia);
			ifa->ifa_addr = SA(&ia->ia_addr);
			ifa->ifa_dstaddr = SA(&ia->ia_dstaddr);
			ifa->ifa_netmask = SA(&ia->ia_sockmask);
			ia->ia_sockmask.sin_len = offsetof(struct sockaddr_in, sin_zero);
			if (ifp->if_flags & IFF_BROADCAST) {
				ia->ia_broadaddr.sin_len = sizeof(ia->ia_addr);
				ia->ia_broadaddr.sin_family = AF_INET;
			}
			ia->ia_ifp = ifp;
			if (!(ifp->if_flags & IFF_LOOPBACK)) {
				in_interfaces++;
			}
			/* if_attach_ifa() holds a reference for ifa_link */
			if_attach_ifa(ifp, ifa);
			/*
			 * If we have to go through in_ifinit(), make sure
			 * to avoid installing route(s) based on this address
			 * via PFC_IFUP event, before the link resolver (ARP)
			 * initializes it.
			 */
			if (cmd == SIOCAIFADDR || cmd == SIOCSIFADDR) {
				ifa->ifa_debug |= IFD_NOTREADY;
			}
			IFA_UNLOCK(ifa);
			ifnet_lock_done(ifp);
			lck_rw_lock_exclusive(&in_ifaddr_rwlock);
			/* Hold a reference for ia_link */
			ifa_addref(ifa);
			TAILQ_INSERT_TAIL(&in_ifaddrhead, ia, ia_link);
			lck_rw_done(&in_ifaddr_rwlock);
			/* discard error */
			(void) in_domifattach(ifp);
			error = 0;
		}
		break;
	}

	switch (cmd) {
	case SIOCGIFDSTADDR:            /* struct ifreq */
	case SIOCSIFDSTADDR:            /* struct ifreq */
		ifr = (struct ifreq *)(void *)data;
		error = inctl_ifdstaddr(ifp, ia, cmd, ifr);
		break;

	case SIOCGIFBRDADDR:            /* struct ifreq */
	case SIOCSIFBRDADDR:            /* struct ifreq */
		ifr = (struct ifreq *)(void *)data;
		error = inctl_ifbrdaddr(ifp, ia, cmd, ifr);
		break;

	case SIOCGIFNETMASK:            /* struct ifreq */
	case SIOCSIFNETMASK:            /* struct ifreq */
		ifr = (struct ifreq *)(void *)data;
		error = inctl_ifnetmask(ifp, ia, cmd, ifr);
		break;

	case SIOCGIFADDR:               /* struct ifreq */
	case SIOCSIFADDR:               /* struct ifreq */
	case SIOCAIFADDR:               /* struct {if,in_}aliasreq */
	case SIOCDIFADDR:               /* struct ifreq */
		ifr = (struct ifreq *)(void *)data;
		error = inctl_ifaddr(ifp, ia, cmd, ifr);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

done:
	if (ia != NULL) {
		if (cmd == SIOCDIFADDR) {
			IFA_LOCK(&ia->ia_ifa);
			ia->ia_ifa.ifa_debug &= ~IFD_DETACHING;
			if (ia->ia_ifa.ifa_del_waiters > 0) {
				ia->ia_ifa.ifa_del_waiters = 0;
				wakeup(ia->ia_ifa.ifa_del_wc);
			}
			IFA_UNLOCK(&ia->ia_ifa);
		}
		ifa_remref(&ia->ia_ifa);
	}
	if (so_unlocked) {
		socket_lock(so, 0);
	}

	return error;
}

/*
 * Delete any existing route for an interface.
 */
void
in_ifscrub(struct ifnet *ifp, struct in_ifaddr *ia, int locked)
{
	IFA_LOCK(&ia->ia_ifa);
	if ((ia->ia_flags & IFA_ROUTE) == 0) {
		IFA_UNLOCK(&ia->ia_ifa);
		return;
	}
	IFA_UNLOCK(&ia->ia_ifa);
	if (!locked) {
		lck_mtx_lock(rnh_lock);
	}
	if (ifp->if_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)) {
		rtinit_locked(&(ia->ia_ifa), RTM_DELETE, RTF_HOST);
	} else {
		rtinit_locked(&(ia->ia_ifa), RTM_DELETE, 0);
	}
	IFA_LOCK(&ia->ia_ifa);
	ia->ia_flags &= ~IFA_ROUTE;
	IFA_UNLOCK(&ia->ia_ifa);
	if (!locked) {
		lck_mtx_unlock(rnh_lock);
	}
}

/*
 * Caller must hold in_ifaddr_rwlock as writer.
 */
static void
in_iahash_remove(struct in_ifaddr *ia)
{
	LCK_RW_ASSERT(&in_ifaddr_rwlock, LCK_RW_ASSERT_EXCLUSIVE);
	IFA_LOCK_ASSERT_HELD(&ia->ia_ifa);

	if (!IA_IS_HASHED(ia)) {
		panic("attempt to remove wrong ia %p from hash table", ia);
		/* NOTREACHED */
	}
	TAILQ_REMOVE(INADDR_HASH(ia->ia_addr.sin_addr.s_addr), ia, ia_hash);
	IA_HASH_INIT(ia);
	ifa_remref(&ia->ia_ifa);
}

/*
 * Caller must hold in_ifaddr_rwlock as writer.
 */
static void
in_iahash_insert(struct in_ifaddr *ia)
{
	LCK_RW_ASSERT(&in_ifaddr_rwlock, LCK_RW_ASSERT_EXCLUSIVE);
	IFA_LOCK_ASSERT_HELD(&ia->ia_ifa);

	if (ia->ia_addr.sin_family != AF_INET) {
		panic("attempt to insert wrong ia %p into hash table", ia);
		/* NOTREACHED */
	} else if (IA_IS_HASHED(ia)) {
		panic("attempt to double-insert ia %p into hash table", ia);
		/* NOTREACHED */
	}
	TAILQ_INSERT_HEAD(INADDR_HASH(ia->ia_addr.sin_addr.s_addr),
	    ia, ia_hash);
	ifa_addref(&ia->ia_ifa);
}

/*
 * Some point to point interfaces that are tunnels borrow the address from
 * an underlying interface (e.g. VPN server). In order for source address
 * selection logic to find the underlying interface first, we add the address
 * of borrowing point to point interfaces at the end of the list.
 * (see rdar://6733789)
 *
 * Caller must hold in_ifaddr_rwlock as writer.
 */
static void
in_iahash_insert_ptp(struct in_ifaddr *ia)
{
	struct in_ifaddr *__single tmp_ifa;
	struct ifnet *__single tmp_ifp;

	LCK_RW_ASSERT(&in_ifaddr_rwlock, LCK_RW_ASSERT_EXCLUSIVE);
	IFA_LOCK_ASSERT_HELD(&ia->ia_ifa);

	if (ia->ia_addr.sin_family != AF_INET) {
		panic("attempt to insert wrong ia %p into hash table", ia);
		/* NOTREACHED */
	} else if (IA_IS_HASHED(ia)) {
		panic("attempt to double-insert ia %p into hash table", ia);
		/* NOTREACHED */
	}
	IFA_UNLOCK(&ia->ia_ifa);
	TAILQ_FOREACH(tmp_ifa, INADDR_HASH(ia->ia_addr.sin_addr.s_addr),
	    ia_hash) {
		IFA_LOCK(&tmp_ifa->ia_ifa);
		/* ia->ia_addr won't change, so check without lock */
		if (IA_SIN(tmp_ifa)->sin_addr.s_addr ==
		    ia->ia_addr.sin_addr.s_addr) {
			IFA_UNLOCK(&tmp_ifa->ia_ifa);
			break;
		}
		IFA_UNLOCK(&tmp_ifa->ia_ifa);
	}
	tmp_ifp = (tmp_ifa == NULL) ? NULL : tmp_ifa->ia_ifp;

	IFA_LOCK(&ia->ia_ifa);
	if (tmp_ifp == NULL) {
		TAILQ_INSERT_HEAD(INADDR_HASH(ia->ia_addr.sin_addr.s_addr),
		    ia, ia_hash);
	} else {
		TAILQ_INSERT_TAIL(INADDR_HASH(ia->ia_addr.sin_addr.s_addr),
		    ia, ia_hash);
	}
	ifa_addref(&ia->ia_ifa);
}

/*
 * Initialize an interface's internet address
 * and routing table entry.
 */
static int
in_ifinit(struct ifnet *ifp, struct in_ifaddr *ia, struct sockaddr_in *sin,
    int scrub)
{
	u_int32_t i = ntohl(sin->sin_addr.s_addr);
	struct sockaddr_in oldaddr;
	int flags = RTF_UP, error;
	struct ifaddr *__single ifa0;
	unsigned int cmd;
	int oldremoved = 0;

	/* Take an extra reference for this routine */
	ifa_addref(&ia->ia_ifa);

	lck_rw_lock_exclusive(&in_ifaddr_rwlock);
	IFA_LOCK(&ia->ia_ifa);
	oldaddr = ia->ia_addr;
	if (IA_IS_HASHED(ia)) {
		oldremoved = 1;
		in_iahash_remove(ia);
	}
	ia->ia_addr = *sin;
	/*
	 * Interface addresses should not contain port or sin_zero information.
	 */
	SIN(&ia->ia_addr)->sin_family = AF_INET;
	SIN(&ia->ia_addr)->sin_len = sizeof(struct sockaddr_in);
	SIN(&ia->ia_addr)->sin_port = 0;
	bzero(&SIN(&ia->ia_addr)->sin_zero, sizeof(sin->sin_zero));
	if ((ifp->if_flags & IFF_POINTOPOINT)) {
		in_iahash_insert_ptp(ia);
	} else {
		in_iahash_insert(ia);
	}
	IFA_UNLOCK(&ia->ia_ifa);
	lck_rw_done(&in_ifaddr_rwlock);

	/*
	 * Give the interface a chance to initialize if this is its first
	 * address, and to validate the address if necessary.  Send down
	 * SIOCSIFADDR for first address, and SIOCAIFADDR for alias(es).
	 * We find the first IPV4 address assigned to it and check if this
	 * is the same as the one passed into this routine.
	 */
	ifa0 = ifa_ifpgetprimary(ifp, AF_INET);
	cmd = (&ia->ia_ifa == ifa0) ? SIOCSIFADDR : SIOCAIFADDR;
	error = ifnet_ioctl(ifp, PF_INET, cmd, ia);
	if (error == EOPNOTSUPP) {
		error = 0;
	}
	/*
	 * If we've just sent down SIOCAIFADDR, send another ioctl down
	 * for SIOCSIFADDR for the first IPV4 address of the interface,
	 * because an address change on one of the addresses will result
	 * in the removal of the previous first IPV4 address.  KDP needs
	 * be reconfigured with the current primary IPV4 address.
	 */
	if (error == 0 && cmd == SIOCAIFADDR) {
		/*
		 * NOTE: SIOCSIFADDR is defined with struct ifreq
		 * as parameter, but here we are sending it down
		 * to the interface with a pointer to struct ifaddr,
		 * for legacy reasons.
		 */
		error = ifnet_ioctl(ifp, PF_INET, SIOCSIFADDR, ifa0);
		if (error == EOPNOTSUPP) {
			error = 0;
		}
	}

	/* Release reference from ifa_ifpgetprimary() */
	ifa_remref(ifa0);

	if (error) {
		lck_rw_lock_exclusive(&in_ifaddr_rwlock);
		IFA_LOCK(&ia->ia_ifa);
		if (IA_IS_HASHED(ia)) {
			in_iahash_remove(ia);
		}
		ia->ia_addr = oldaddr;
		if (oldremoved) {
			if ((ifp->if_flags & IFF_POINTOPOINT)) {
				in_iahash_insert_ptp(ia);
			} else {
				in_iahash_insert(ia);
			}
		}
		IFA_UNLOCK(&ia->ia_ifa);
		lck_rw_done(&in_ifaddr_rwlock);
		/* Release extra reference taken above */
		ifa_remref(&ia->ia_ifa);
		return error;
	}
	lck_mtx_lock(rnh_lock);
	IFA_LOCK(&ia->ia_ifa);
	/*
	 * Address has been initialized by the link resolver (ARP)
	 * via ifnet_ioctl() above; it may now generate route(s).
	 */
	ia->ia_ifa.ifa_debug &= ~IFD_NOTREADY;
	if (scrub) {
		ia->ia_ifa.ifa_addr = SA(&oldaddr);
		IFA_UNLOCK(&ia->ia_ifa);
		in_ifscrub(ifp, ia, 1);
		IFA_LOCK(&ia->ia_ifa);
		ia->ia_ifa.ifa_addr = SA(&ia->ia_addr);
	}
	IFA_LOCK_ASSERT_HELD(&ia->ia_ifa);
	if (IN_CLASSA(i)) {
		ia->ia_netmask = IN_CLASSA_NET;
	} else if (IN_CLASSB(i)) {
		ia->ia_netmask = IN_CLASSB_NET;
	} else {
		ia->ia_netmask = IN_CLASSC_NET;
	}
	/*
	 * The subnet mask usually includes at least the standard network part,
	 * but may may be smaller in the case of supernetting.
	 * If it is set, we believe it.
	 */
	if (ia->ia_subnetmask == 0) {
		ia->ia_subnetmask = ia->ia_netmask;
		ia->ia_sockmask.sin_addr.s_addr = htonl(ia->ia_subnetmask);
	} else {
		ia->ia_netmask &= ia->ia_subnetmask;
	}
	ia->ia_net = i & ia->ia_netmask;
	ia->ia_subnet = i & ia->ia_subnetmask;
	in_socktrim(&ia->ia_sockmask);
	/*
	 * Add route for the network.
	 */
	ia->ia_ifa.ifa_metric = ifp->if_metric;
	if (ifp->if_flags & IFF_BROADCAST) {
		ia->ia_broadaddr.sin_addr.s_addr =
		    htonl(ia->ia_subnet | ~ia->ia_subnetmask);
		ia->ia_netbroadcast.s_addr =
		    htonl(ia->ia_net | ~ia->ia_netmask);
	} else if (ifp->if_flags & IFF_LOOPBACK) {
		ia->ia_ifa.ifa_dstaddr = ia->ia_ifa.ifa_addr;
		flags |= RTF_HOST;
	} else if (ifp->if_flags & IFF_POINTOPOINT) {
		if (ia->ia_dstaddr.sin_family != AF_INET) {
			IFA_UNLOCK(&ia->ia_ifa);
			lck_mtx_unlock(rnh_lock);
			/* Release extra reference taken above */
			ifa_remref(&ia->ia_ifa);
			return 0;
		}
		ia->ia_dstaddr.sin_len = sizeof(struct sockaddr_in);
		flags |= RTF_HOST;
	}
	IFA_UNLOCK(&ia->ia_ifa);

	if ((error = rtinit_locked(&(ia->ia_ifa), RTM_ADD, flags)) == 0) {
		IFA_LOCK(&ia->ia_ifa);
		ia->ia_flags |= IFA_ROUTE;
		IFA_UNLOCK(&ia->ia_ifa);
	}
	lck_mtx_unlock(rnh_lock);

	/* XXX check if the subnet route points to the same interface */
	if (error == EEXIST) {
		error = 0;
	}

	/*
	 * If the interface supports multicast, join the "all hosts"
	 * multicast group on that interface.
	 */
	if (ifp->if_flags & IFF_MULTICAST) {
		struct in_addr addr;

		lck_mtx_lock(&ifp->if_addrconfig_lock);
		addr.s_addr = htonl(INADDR_ALLHOSTS_GROUP);
		if (ifp->if_allhostsinm == NULL) {
			struct in_multi *__single inm;
			inm = in_addmulti(&addr, ifp);

			if (inm != NULL) {
				/*
				 * Keep the reference on inm added by
				 * in_addmulti above for storing the
				 * pointer in allhostsinm.
				 */
				ifp->if_allhostsinm = inm;
			} else {
				printf("%s: failed to add membership to "
				    "all-hosts multicast address on %s\n",
				    __func__, if_name(ifp));
			}
		}
		lck_mtx_unlock(&ifp->if_addrconfig_lock);
	}

	/* Release extra reference taken above */
	ifa_remref(&ia->ia_ifa);

	if (error == 0) {
		/* invalidate route caches */
		routegenid_inet_update();
	}

	return error;
}

/*
 * Return TRUE if the address might be a local broadcast address.
 */
boolean_t
in_broadcast(struct in_addr in, struct ifnet *ifp)
{
	struct ifaddr *__single ifa;
	u_int32_t t;

	if (in.s_addr == INADDR_BROADCAST || in.s_addr == INADDR_ANY) {
		return TRUE;
	}
	if (!(ifp->if_flags & IFF_BROADCAST)) {
		return FALSE;
	}
	t = ntohl(in.s_addr);

	/*
	 * Look through the list of addresses for a match
	 * with a broadcast address.
	 */
	ifnet_lock_shared(ifp);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		struct in_ifaddr *ia = ifatoia(ifa);
		IFA_LOCK(ifa);
		if (ifa->ifa_addr->sa_family == AF_INET &&
		    (in.s_addr == ia->ia_broadaddr.sin_addr.s_addr ||
		    in.s_addr == ia->ia_netbroadcast.s_addr ||
		    /*
		     * Check for old-style (host 0) broadcast.
		     */
		    t == ia->ia_subnet || t == ia->ia_net) &&
		    /*
		     * Check for an all one subnetmask. These
		     * only exist when an interface gets a secondary
		     * address.
		     */
		    ia->ia_subnetmask != (u_int32_t)0xffffffff) {
			IFA_UNLOCK(ifa);
			ifnet_lock_done(ifp);
			return TRUE;
		}
		IFA_UNLOCK(ifa);
	}
	ifnet_lock_done(ifp);
	return FALSE;
#undef ia
}

void
in_purgeaddrs(struct ifnet *ifp)
{
	uint16_t addresses_count = 0;
	struct ifaddr **__counted_by(addresses_count) ifap = NULL;
	int err, i;

	VERIFY(ifp != NULL);

	/*
	 * Be nice, and try the civilized way first.  If we can't get
	 * rid of them this way, then do it the rough way.  We must
	 * only get here during detach time, after the ifnet has been
	 * removed from the global list and arrays.
	 */
	err = ifnet_get_address_list_family_internal(ifp, &ifap, &addresses_count,
	    AF_INET, 1, M_WAITOK, 0);
	if (err == 0 && ifap != NULL) {
		struct ifreq ifr;

		bzero(&ifr, sizeof(ifr));
		(void) snprintf(ifr.ifr_name, sizeof(ifr.ifr_name),
		    "%s", if_name(ifp));

		for (i = 0; ifap[i] != NULL; i++) {
			struct ifaddr *__single ifa;

			ifa = ifap[i];
			IFA_LOCK(ifa);
			SOCKADDR_COPY(ifa->ifa_addr, &ifr.ifr_addr,
			    sizeof(struct sockaddr_in));
			IFA_UNLOCK(ifa);
			err = in_control(NULL, SIOCDIFADDR, (caddr_t)&ifr, ifp,
			    kernproc);
			/* if we lost the race, ignore it */
			if (err == EADDRNOTAVAIL) {
				err = 0;
			}
			if (err != 0) {
				char s_addr[MAX_IPv4_STR_LEN];
				char s_dstaddr[MAX_IPv4_STR_LEN];
				struct in_addr *__single s, *d;

				IFA_LOCK(ifa);
				s = &SIN(ifa->ifa_addr)->sin_addr;
				d = &SIN(ifa->ifa_dstaddr)->sin_addr;
				(void) inet_ntop(AF_INET, &s->s_addr, s_addr,
				    sizeof(s_addr));
				(void) inet_ntop(AF_INET, &d->s_addr, s_dstaddr,
				    sizeof(s_dstaddr));
				IFA_UNLOCK(ifa);

				printf("%s: SIOCDIFADDR ifp=%s ifa_addr=%s "
				    "ifa_dstaddr=%s (err=%d)\n", __func__,
				    ifp->if_xname, s_addr, s_dstaddr, err);
			}
		}
		ifnet_address_list_free_counted_by(ifap, addresses_count);
	} else if (err != 0 && err != ENXIO) {
		printf("%s: error retrieving list of AF_INET addresses for "
		    "ifp=%s (err=%d)\n", __func__, ifp->if_xname, err);
	}
}

static struct in_ifaddr *
in_ifaddr_alloc(void)
{
	struct in_ifaddr *__single inifa;

	inifa = kalloc_type(struct in_ifaddr, Z_ZERO | Z_WAITOK);
	if (inifa == NULL) {
		return NULL;
	}

	inifa->ia_ifa.ifa_free = in_ifaddr_free;
	inifa->ia_ifa.ifa_debug |= IFD_ALLOC;
	inifa->ia_ifa.ifa_del_wc = &inifa->ia_ifa.ifa_debug;
	inifa->ia_ifa.ifa_del_waiters = 0;
	ifa_lock_init(&inifa->ia_ifa);
	ifa_initref(&inifa->ia_ifa);

	return inifa;
}

static void
in_ifaddr_free(struct ifaddr *ifa)
{
	struct in_ifaddr *__single inifa = ifatoia(ifa);

	IFA_LOCK_ASSERT_HELD(ifa);

	if (__improbable(!(ifa->ifa_debug & IFD_ALLOC))) {
		panic("%s: ifa %p cannot be freed", __func__, ifa);
		/* NOTREACHED */
	}
	IFA_UNLOCK(ifa);
	ifa_lock_destroy(ifa);

	kfree_type(struct in_ifaddr, inifa);
}

/*
 * Handle SIOCGASSOCIDS ioctl for PF_INET domain.
 */
static int
in_getassocids(struct socket *so, uint32_t *cnt, user_addr_t aidp)
{
	struct inpcb *__single inp = sotoinpcb(so);
	sae_associd_t aid;

	if (inp == NULL || inp->inp_state == INPCB_STATE_DEAD) {
		return EINVAL;
	}

	/* INPCB has no concept of association */
	aid = SAE_ASSOCID_ANY;
	*cnt = 0;

	/* just asking how many there are? */
	if (aidp == USER_ADDR_NULL) {
		return 0;
	}

	return copyout(&aid, aidp, sizeof(aid));
}

/*
 * Handle SIOCGCONNIDS ioctl for PF_INET domain.
 */
static int
in_getconnids(struct socket *so, sae_associd_t aid, uint32_t *cnt,
    user_addr_t cidp)
{
	struct inpcb *__single inp = sotoinpcb(so);
	sae_connid_t cid;

	if (inp == NULL || inp->inp_state == INPCB_STATE_DEAD) {
		return EINVAL;
	}

	if (aid != SAE_ASSOCID_ANY && aid != SAE_ASSOCID_ALL) {
		return EINVAL;
	}

	/* if connected, return 1 connection count */
	*cnt = ((so->so_state & SS_ISCONNECTED) ? 1 : 0);

	/* just asking how many there are? */
	if (cidp == USER_ADDR_NULL) {
		return 0;
	}

	/* if INPCB is connected, assign it connid 1 */
	cid = ((*cnt != 0) ? 1 : SAE_CONNID_ANY);

	return copyout(&cid, cidp, sizeof(cid));
}

/*
 * Handle SIOCGCONNINFO ioctl for PF_INET domain.
 */
int
in_getconninfo(struct socket *so, sae_connid_t cid, uint32_t *flags,
    uint32_t *ifindex, int32_t *soerror, user_addr_t src, socklen_t *src_len,
    user_addr_t dst, socklen_t *dst_len, uint32_t *aux_type,
    user_addr_t aux_data, uint32_t *aux_len)
{
	struct inpcb *__single inp = sotoinpcb(so);
	struct sockaddr_in sin;
	struct ifnet *__single ifp = NULL;
	int error = 0;
	u_int32_t copy_len = 0;

	/*
	 * Don't test for INPCB_STATE_DEAD since this may be called
	 * after SOF_PCBCLEARING is set, e.g. after tcp_close().
	 */
	if (inp == NULL) {
		error = EINVAL;
		goto out;
	}

	if (cid != SAE_CONNID_ANY && cid != SAE_CONNID_ALL && cid != 1) {
		error = EINVAL;
		goto out;
	}

	ifp = inp->inp_last_outifp;
	*ifindex = ((ifp != NULL) ? ifp->if_index : 0);
	*soerror = so->so_error;
	*flags = 0;
	if (so->so_state & SS_ISCONNECTED) {
		*flags |= (CIF_CONNECTED | CIF_PREFERRED);
	}
	if (inp->inp_flags & INP_BOUND_IF) {
		*flags |= CIF_BOUND_IF;
	}
	if (!(inp->inp_flags & INP_INADDR_ANY)) {
		*flags |= CIF_BOUND_IP;
	}
	if (!(inp->inp_flags & INP_ANONPORT)) {
		*flags |= CIF_BOUND_PORT;
	}

	SOCKADDR_ZERO(&sin, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;

	/* source address and port */
	sin.sin_port = inp->inp_lport;
	sin.sin_addr.s_addr = inp->inp_laddr.s_addr;
	if (*src_len == 0) {
		*src_len = sin.sin_len;
	} else {
		if (src != USER_ADDR_NULL) {
			copy_len = min(*src_len, sizeof(sin));
			error = copyout(&sin, src, copy_len);
			if (error != 0) {
				goto out;
			}
			*src_len = copy_len;
		}
	}

	/* destination address and port */
	sin.sin_port = inp->inp_fport;
	sin.sin_addr.s_addr = inp->inp_faddr.s_addr;
	if (*dst_len == 0) {
		*dst_len = sin.sin_len;
	} else {
		if (dst != USER_ADDR_NULL) {
			copy_len = min(*dst_len, sizeof(sin));
			error = copyout(&sin, dst, copy_len);
			if (error != 0) {
				goto out;
			}
			*dst_len = copy_len;
		}
	}

	if (SOCK_PROTO(so) == IPPROTO_TCP) {
		struct conninfo_tcp tcp_ci;

		*aux_type = CIAUX_TCP;
		if (*aux_len == 0) {
			*aux_len = sizeof(tcp_ci);
		} else {
			if (aux_data != USER_ADDR_NULL) {
				copy_len = min(*aux_len, sizeof(tcp_ci));
				bzero(&tcp_ci, sizeof(tcp_ci));
				tcp_getconninfo(so, &tcp_ci);
				error = copyout(&tcp_ci, aux_data, copy_len);
				if (error != 0) {
					goto out;
				}
				*aux_len = copy_len;
			}
		}
	} else {
		*aux_type = 0;
		*aux_len = 0;
	}

out:
	return error;
}

struct in_ifaddr*
inifa_ifpwithflag(struct ifnet * ifp, uint32_t flag)
{
	struct ifaddr *__single ifa;

	ifnet_lock_shared(ifp);
	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_link)
	{
		IFA_LOCK_SPIN(ifa);
		if (ifa->ifa_addr->sa_family != AF_INET) {
			IFA_UNLOCK(ifa);
			continue;
		}
		if (((ifatoia(ifa))->ia_flags & flag) == flag) {
			ifa_addref(ifa);
			IFA_UNLOCK(ifa);
			break;
		}
		IFA_UNLOCK(ifa);
	}
	ifnet_lock_done(ifp);

	return ifatoia(ifa);
}

struct in_ifaddr *
inifa_ifpclatv4(struct ifnet * ifp)
{
	struct ifaddr *__single ifa;

	ifnet_lock_shared(ifp);
	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_link)
	{
		uint32_t addr = 0;
		IFA_LOCK_SPIN(ifa);
		if (ifa->ifa_addr->sa_family != AF_INET) {
			IFA_UNLOCK(ifa);
			continue;
		}

		addr = ntohl(SIN(ifa->ifa_addr)->sin_addr.s_addr);
		if (!IN_LINKLOCAL(addr) &&
		    !IN_LOOPBACK(addr)) {
			ifa_addref(ifa);
			IFA_UNLOCK(ifa);
			break;
		}
		IFA_UNLOCK(ifa);
	}
	ifnet_lock_done(ifp);

	return ifatoia(ifa);
}

/*
 * IPPROTO_xxx.
 *
 * The switch statement below does nothing at runtime, as it serves as a
 * compile time check to ensure that all of the IPPROTO_xxx constants are
 * unique.  This works as long as this routine gets updated each time a
 * new IPPROTO_xxx constant gets added.
 *
 * Any failures at compile time indicates duplicated IPPROTO_xxx values.
 */
static __attribute__((unused)) void
ipproto_cassert(void)
{
	/*
	 * This is equivalent to _CASSERT() and the compiler wouldn't
	 * generate any instructions, thus for compile time only.
	 */
	switch ((u_int16_t)0) {
	/* bsd/netinet/in.h */
	case IPPROTO_IP:
//	case IPPROTO_HOPOPTS: // same value as IPPROTO_IP
	case IPPROTO_ICMP:
	case IPPROTO_IGMP:
	case IPPROTO_GGP:
	case IPPROTO_IPV4:
	// #define IPPROTO_IPIP            IPPROTO_IPV4
	case IPPROTO_TCP:
	case IPPROTO_ST:
	case IPPROTO_EGP:
	case IPPROTO_PIGP:
	case IPPROTO_RCCMON:
	case IPPROTO_NVPII:
	case IPPROTO_PUP:
	case IPPROTO_ARGUS:
	case IPPROTO_EMCON:
	case IPPROTO_XNET:
	case IPPROTO_CHAOS:
	case IPPROTO_UDP:
	case IPPROTO_MUX:
	case IPPROTO_MEAS:
	case IPPROTO_HMP:
	case IPPROTO_PRM:
	case IPPROTO_IDP:
	case IPPROTO_TRUNK1:
	case IPPROTO_TRUNK2:
	case IPPROTO_LEAF1:
	case IPPROTO_LEAF2:
	case IPPROTO_RDP:
	case IPPROTO_IRTP:
	case IPPROTO_TP:
	case IPPROTO_BLT:
	case IPPROTO_NSP:
	case IPPROTO_INP:
	case IPPROTO_SEP:
	case IPPROTO_3PC:
	case IPPROTO_IDPR:
	case IPPROTO_XTP:
	case IPPROTO_DDP:
	case IPPROTO_CMTP:
	case IPPROTO_TPXX:
	case IPPROTO_IL:
	case IPPROTO_IPV6:
	case IPPROTO_SDRP:
	case IPPROTO_ROUTING:
	case IPPROTO_FRAGMENT:
	case IPPROTO_IDRP:
	case IPPROTO_RSVP:
	case IPPROTO_GRE:
	case IPPROTO_MHRP:
	case IPPROTO_BHA:
	case IPPROTO_ESP:
	case IPPROTO_AH:
	case IPPROTO_INLSP:
	case IPPROTO_SWIPE:
	case IPPROTO_NHRP:
	case IPPROTO_ICMPV6:
	case IPPROTO_NONE:
	case IPPROTO_DSTOPTS:
	case IPPROTO_AHIP:
	case IPPROTO_CFTP:
	case IPPROTO_HELLO:
	case IPPROTO_SATEXPAK:
	case IPPROTO_KRYPTOLAN:
	case IPPROTO_RVD:
	case IPPROTO_IPPC:
	case IPPROTO_ADFS:
	case IPPROTO_SATMON:
	case IPPROTO_VISA:
	case IPPROTO_IPCV:
	case IPPROTO_CPNX:
	case IPPROTO_CPHB:
	case IPPROTO_WSN:
	case IPPROTO_PVP:
	case IPPROTO_BRSATMON:
	case IPPROTO_ND:
	case IPPROTO_WBMON:
	case IPPROTO_WBEXPAK:
	case IPPROTO_EON:
	case IPPROTO_VMTP:
	case IPPROTO_SVMTP:
	case IPPROTO_VINES:
	case IPPROTO_TTP:
	case IPPROTO_IGP:
	case IPPROTO_DGP:
	case IPPROTO_TCF:
	case IPPROTO_IGRP:
	case IPPROTO_OSPFIGP:
	case IPPROTO_SRPC:
	case IPPROTO_LARP:
	case IPPROTO_MTP:
	case IPPROTO_AX25:
	case IPPROTO_IPEIP:
	case IPPROTO_MICP:
	case IPPROTO_SCCSP:
	case IPPROTO_ETHERIP:
	case IPPROTO_ENCAP:
	case IPPROTO_APES:
	case IPPROTO_GMTP:
	case IPPROTO_PIM:
	case IPPROTO_IPCOMP:
	case IPPROTO_PGM:
	case IPPROTO_SCTP:
	case IPPROTO_DIVERT:
	case IPPROTO_RAW:
	case IPPROTO_MAX:
	case IPPROTO_DONE:

	/* bsd/netinet/in_private.h */
	case IPPROTO_QUIC:
		;
	}
}

static __attribute__((unused)) void
ipsockopt_cassert(void)
{
	switch ((int)0) {
	case 0:

	/* bsd/netinet/in.h */
	case IP_OPTIONS:
	case IP_HDRINCL:
	case IP_TOS:
	case IP_TTL:
	case IP_RECVOPTS:
	case IP_RECVRETOPTS:
	case IP_RECVDSTADDR:
	case IP_RETOPTS:
	case IP_MULTICAST_IF:
	case IP_MULTICAST_TTL:
	case IP_MULTICAST_LOOP:
	case IP_ADD_MEMBERSHIP:
	case IP_DROP_MEMBERSHIP:
	case IP_MULTICAST_VIF:
	case IP_RSVP_ON:
	case IP_RSVP_OFF:
	case IP_RSVP_VIF_ON:
	case IP_RSVP_VIF_OFF:
	case IP_PORTRANGE:
	case IP_RECVIF:
	case IP_IPSEC_POLICY:
	case IP_FAITH:
#ifdef __APPLE__
	case IP_STRIPHDR:
#endif
	case IP_RECVTTL:
	case IP_BOUND_IF:
	case IP_PKTINFO:
// #define IP_RECVPKTINFO          IP_PKTINFO
	case IP_RECVTOS:
	case IP_DONTFRAG:
	case IP_FW_ADD:
	case IP_FW_DEL:
	case IP_FW_FLUSH:
	case IP_FW_ZERO:
	case IP_FW_GET:
	case IP_FW_RESETLOG:
	case IP_OLD_FW_ADD:
	case IP_OLD_FW_DEL:
	case IP_OLD_FW_FLUSH:
	case IP_OLD_FW_ZERO:
	case IP_OLD_FW_GET:
	case IP_NAT__XXX:
	case IP_OLD_FW_RESETLOG:
	case IP_DUMMYNET_CONFIGURE:
	case IP_DUMMYNET_DEL:
	case IP_DUMMYNET_FLUSH:
	case IP_DUMMYNET_GET:
	case IP_TRAFFIC_MGT_BACKGROUND:
	case IP_MULTICAST_IFINDEX:
	case IP_ADD_SOURCE_MEMBERSHIP:
	case IP_DROP_SOURCE_MEMBERSHIP:
	case IP_BLOCK_SOURCE:
	case IP_UNBLOCK_SOURCE:
	case IP_MSFILTER:
	case MCAST_JOIN_GROUP:
	case MCAST_LEAVE_GROUP:
	case MCAST_JOIN_SOURCE_GROUP:
	case MCAST_LEAVE_SOURCE_GROUP:
	case MCAST_BLOCK_SOURCE:
	case MCAST_UNBLOCK_SOURCE:

	/* bsd/netinet/in_private.h */
	case IP_NO_IFT_CELLULAR:
// #define IP_NO_IFT_PDP           IP_NO_IFT_CELLULAR /* deprecated */
	case IP_OUT_IF:
		;
	}
}
