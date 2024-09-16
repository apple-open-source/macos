/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
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

/*	$FreeBSD: src/sys/net/if_stf.c,v 1.1.2.6 2001/07/24 19:10:18 brooks Exp $	*/
/*	$KAME: if_stf.c,v 1.62 2001/06/07 22:32:16 itojun Exp $	*/

/*
 * Copyright (C) 2000 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * NOTICE: This file was modified by SPARTA, Inc. in 2006 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 */

/*
 * 6to4 interface, based on RFC3056.
 *
 * 6to4 interface is NOT capable of link-layer (I mean, IPv4) multicasting.
 * There is no address mapping defined from IPv6 multicast address to IPv4
 * address.  Therefore, we do not have IFF_MULTICAST on the interface.
 *
 * Due to the lack of address mapping for link-local addresses, we cannot
 * throw packets toward link-local addresses (fe80::x).  Also, we cannot throw
 * packets to link-local multicast addresses (ff02::x).
 *
 * Here are interesting symptoms due to the lack of link-local address:
 *
 * Unicast routing exchange:
 * - RIPng: Impossible.  Uses link-local multicast packet toward ff02::9,
 *   and link-local addresses as nexthop.
 * - OSPFv6: Impossible.  OSPFv6 assumes that there's link-local address
 *   assigned to the link, and makes use of them.  Also, HELLO packets use
 *   link-local multicast addresses (ff02::5 and ff02::6).
 * - BGP4+: Maybe.  You can only use global address as nexthop, and global
 *   address as TCP endpoint address.
 *
 * Multicast routing protocols:
 * - PIM: Hello packet cannot be used to discover adjacent PIM routers.
 *   Adjacent PIM routers must be configured manually (is it really spec-wise
 *   correct thing to do?).
 *
 * ICMPv6:
 * - Redirects cannot be used due to the lack of link-local address.
 *
 * stf interface does not have, and will not need, a link-local address.
 * It seems to have no real benefit and does not help the above symptoms much.
 * Even if we assign link-locals to interface, we cannot really
 * use link-local unicast/multicast on top of 6to4 cloud (since there's no
 * encapsulation defined for link-local address), and the above analysis does
 * not change.  RFC3056 does not mandate the assignment of link-local address
 * either.
 *
 * 6to4 interface has security issues.  Refer to
 * http://playground.iijlab.net/i-d/draft-itojun-ipv6-transition-abuse-00.txt
 * for details.  The code tries to filter out some of malicious packets.
 * Note that there is no way to be 100% secure.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <sys/malloc.h>

#include <kern/locks.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>

#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_var.h>
#include <netinet/ip_ecn.h>

#include <netinet/ip_encap.h>
#include <net/kpi_interface.h>
#include <net/kpi_protocol.h>


#include <net/net_osdep.h>

#include <net/bpf.h>

#include <net/sockaddr_utils.h>

#define GET_V4(x) ((const struct in_addr *)(const void *)(&(x)->s6_addr16[1]))

static LCK_GRP_DECLARE(stf_mtx_grp, "stf");

struct stf_softc {
	ifnet_t                         sc_if;     /* common area */
	u_int32_t                               sc_protocol_family; /* dlil protocol attached */
	union {
		struct route  __sc_ro4;
		struct route_in6 __sc_ro6; /* just for safety */
	} __sc_ro46;
#define sc_ro   __sc_ro46.__sc_ro4
	decl_lck_mtx_data(, sc_ro_mtx);
	const struct encaptab *encap_cookie;
	bpf_tap_mode            tap_mode;
	bpf_packet_func         tap_callback;
};

void stfattach(void);

static int ip_stf_ttl = 40;

static void in_stf_input(struct mbuf *, int);

static struct protosw in_stf_protosw =
{
	.pr_type =              SOCK_RAW,
	.pr_protocol =          IPPROTO_IPV6,
	.pr_flags =             PR_ATOMIC | PR_ADDR,
	.pr_input =             in_stf_input,
	.pr_ctloutput =         rip_ctloutput,
	.pr_usrreqs =           &rip_usrreqs,
	.pr_unlock =            rip_unlock,
};

static int stf_encapcheck(const struct mbuf *, int, int, void *);
static struct in6_ifaddr *stf_getsrcifa6(struct ifnet *);
int stf_pre_output(struct ifnet *, protocol_family_t, struct mbuf **,
    const struct sockaddr *, void *, char *, char *);
static int stf_checkaddr4(struct stf_softc *, const struct in_addr *,
    struct ifnet *);
static int stf_checkaddr6(struct stf_softc *, struct in6_addr *,
    struct ifnet *);
static void stf_rtrequest(int, struct rtentry *, struct sockaddr *);
static errno_t stf_ioctl(ifnet_t ifp, u_long cmd, void *__sized_by(IOCPARM_LEN(cmd)) data);
static errno_t stf_output(ifnet_t ifp, mbuf_t m);

/*
 * gif_input is the input handler for IP and IPv6 attached to gif
 */
static errno_t
stf_media_input(
	__unused ifnet_t        ifp,
	protocol_family_t       protocol_family,
	mbuf_t                          m,
	__unused char           *frame_header)
{
	if (proto_input(protocol_family, m) != 0) {
		m_freem(m);
	}

	return 0;
}



static errno_t
stf_add_proto(
	ifnet_t                                                                 ifp,
	protocol_family_t                                               protocol_family,
	__unused const struct ifnet_demux_desc  *demux_array,
	__unused u_int32_t                                              demux_count)
{
	/* Only one protocol may be attached at a time */
	struct stf_softc *__single stf = ifnet_softc(ifp);
	if (stf->sc_protocol_family == 0) {
		stf->sc_protocol_family = protocol_family;
	} else {
		printf("stf_add_proto: stf already has a proto\n");
		return EBUSY;
	}

	return 0;
}

static errno_t
stf_del_proto(
	ifnet_t                         ifp,
	protocol_family_t       protocol_family)
{
	if (((struct stf_softc*)ifnet_softc(ifp))->sc_protocol_family == protocol_family) {
		((struct stf_softc*)ifnet_softc(ifp))->sc_protocol_family = 0;
	}

	return 0;
}

static errno_t
stf_attach_inet6(
	ifnet_t                         ifp,
	protocol_family_t       protocol_family)
{
	struct ifnet_attach_proto_param     reg;
	errno_t                                                     stat;

	if (protocol_family != PF_INET6) {
		return EPROTONOSUPPORT;
	}

	bzero(&reg, sizeof(reg));
	reg.input = stf_media_input;
	reg.pre_output = stf_pre_output;

	stat = ifnet_attach_protocol(ifp, protocol_family, &reg);
	if (stat && stat != EEXIST) {
		printf("stf_attach_proto_family can't attach interface fam=%d\n",
		    protocol_family);
	}

	return stat;
}

static errno_t
stf_demux(
	ifnet_t                                 ifp,
	__unused mbuf_t                 m,
	__unused char                   *frame_ptr,
	protocol_family_t               *protocol_family)
{
	struct stf_softc *__single stf = ifnet_softc(ifp);

	*protocol_family = stf->sc_protocol_family;
	return 0;
}

static errno_t
stf_set_bpf_tap(
	ifnet_t                 ifp,
	bpf_tap_mode    mode,
	bpf_packet_func callback)
{
	struct stf_softc *__single sc = ifnet_softc(ifp);

	sc->tap_mode = mode;
	sc->tap_callback = callback;

	return 0;
}

void
stfattach(void)
{
	struct stf_softc *__single sc;
	int error;
	const struct encaptab *__single p;
	struct ifnet_init_eparams stf_init;

	error = proto_register_plumber(PF_INET6, APPLE_IF_FAM_STF,
	    stf_attach_inet6, NULL);
	if (error != 0) {
		printf("proto_register_plumber failed for AF_INET6 error=%d\n", error);
	}

	sc = kalloc_type(struct stf_softc, Z_WAITOK_ZERO_NOFAIL);
	lck_mtx_init(&sc->sc_ro_mtx, &stf_mtx_grp, LCK_ATTR_NULL);

	p = encap_attach_func(AF_INET, IPPROTO_IPV6, stf_encapcheck,
	    &in_stf_protosw, sc);
	if (p == NULL) {
		printf("sftattach encap_attach_func failed\n");
		goto free_sc;
	}
	sc->encap_cookie = p;

	bzero(&stf_init, sizeof(stf_init));
	stf_init.ver = IFNET_INIT_CURRENT_VERSION;
	stf_init.len = sizeof(stf_init);
	stf_init.flags = IFNET_INIT_LEGACY;
	stf_init.name = "stf";
	stf_init.unit = 0;
	stf_init.type = IFT_STF;
	stf_init.family = IFNET_FAMILY_STF;
	stf_init.output = stf_output;
	stf_init.demux = stf_demux;
	stf_init.add_proto = stf_add_proto;
	stf_init.del_proto = stf_del_proto;
	stf_init.softc = sc;
	stf_init.ioctl = stf_ioctl;
	stf_init.set_bpf_tap = stf_set_bpf_tap;

	error = ifnet_allocate_extended(&stf_init, &sc->sc_if);
	if (error != 0) {
		printf("stfattach, ifnet_allocate failed - %d\n", error);
		encap_detach(sc->encap_cookie);
		goto free_sc;
	}
	ifnet_set_mtu(sc->sc_if, IPV6_MMTU);
	ifnet_set_flags(sc->sc_if, 0, 0xffff); /* clear all flags */
#if 0
	/* turn off ingress filter */
	ifnet_set_flags(sc->sc_if, IFF_LINK2, IFF_LINK2);
#endif

	error = ifnet_attach(sc->sc_if, NULL);
	if (error != 0) {
		printf("stfattach: ifnet_attach returned error=%d\n", error);
		encap_detach(sc->encap_cookie);
		ifnet_release(sc->sc_if);
		goto free_sc;
	}

	bpfattach(sc->sc_if, DLT_NULL, sizeof(u_int));

	return;

free_sc:
	lck_mtx_destroy(&sc->sc_ro_mtx, &stf_mtx_grp);
	kfree_type(struct stf_softc, sc);
}

static int
stf_encapcheck(
	const struct mbuf *m,
	__unused int off,
	int proto,
	void *arg)
{
	struct ip ip;
	struct in6_ifaddr *__single ia6;
	struct stf_softc *__single  sc;
	struct in_addr a, b;

	sc = (struct stf_softc *)arg;
	if (sc == NULL) {
		return 0;
	}

	if ((ifnet_flags(sc->sc_if) & IFF_UP) == 0) {
		return 0;
	}

	/* IFF_LINK0 means "no decapsulation" */
	if ((ifnet_flags(sc->sc_if) & IFF_LINK0) != 0) {
		return 0;
	}

	if (proto != IPPROTO_IPV6) {
		return 0;
	}

	mbuf_copydata(__DECONST(struct mbuf *, m), 0, sizeof(ip), &ip);

	if (ip.ip_v != 4) {
		return 0;
	}

	ia6 = stf_getsrcifa6(sc->sc_if);
	if (ia6 == NULL) {
		return 0;
	}

	/*
	 * check if IPv4 dst matches the IPv4 address derived from the
	 * local 6to4 address.
	 * success on: dst = 10.1.1.1, ia6->ia_addr = 2002:0a01:0101:...
	 */
	IFA_LOCK(&ia6->ia_ifa);
	if (bcmp(GET_V4(&ia6->ia_addr.sin6_addr), &ip.ip_dst,
	    sizeof(ip.ip_dst)) != 0) {
		IFA_UNLOCK(&ia6->ia_ifa);
		ifa_remref(&ia6->ia_ifa);
		return 0;
	}
	/*
	 * check if IPv4 src matches the IPv4 address derived from the
	 * local 6to4 address masked by prefixmask.
	 * success on: src = 10.1.1.1, ia6->ia_addr = 2002:0a00:.../24
	 * fail on: src = 10.1.1.1, ia6->ia_addr = 2002:0b00:.../24
	 */
	bzero(&a, sizeof(a));
	a.s_addr = GET_V4(&ia6->ia_addr.sin6_addr)->s_addr;
	a.s_addr &= GET_V4(&ia6->ia_prefixmask.sin6_addr)->s_addr;
	b = ip.ip_src;
	b.s_addr &= GET_V4(&ia6->ia_prefixmask.sin6_addr)->s_addr;
	if (a.s_addr != b.s_addr) {
		IFA_UNLOCK(&ia6->ia_ifa);
		ifa_remref(&ia6->ia_ifa);
		return 0;
	}
	/* stf interface makes single side match only */
	IFA_UNLOCK(&ia6->ia_ifa);
	ifa_remref(&ia6->ia_ifa);
	return 32;
}

static struct in6_ifaddr *
stf_getsrcifa6(struct ifnet *ifp)
{
	struct ifaddr *__single ia;
	struct in_ifaddr *__single ia4;
	struct sockaddr_in6 *__single sin6;
	struct in_addr in;

	ifnet_lock_shared(ifp);
	for (ia = ifp->if_addrlist.tqh_first; ia; ia = ia->ifa_list.tqe_next) {
		IFA_LOCK(ia);
		if (ia->ifa_addr == NULL) {
			IFA_UNLOCK(ia);
			continue;
		}
		if (ia->ifa_addr->sa_family != AF_INET6) {
			IFA_UNLOCK(ia);
			continue;
		}
		sin6 = SIN6(ia->ifa_addr);
		if (!IN6_IS_ADDR_6TO4(&sin6->sin6_addr)) {
			IFA_UNLOCK(ia);
			continue;
		}
		bcopy(GET_V4(&sin6->sin6_addr), &in, sizeof(in));
		IFA_UNLOCK(ia);
		lck_rw_lock_shared(&in_ifaddr_rwlock);
		for (ia4 = TAILQ_FIRST(&in_ifaddrhead);
		    ia4;
		    ia4 = TAILQ_NEXT(ia4, ia_link)) {
			IFA_LOCK(&ia4->ia_ifa);
			if (ia4->ia_addr.sin_addr.s_addr == in.s_addr) {
				IFA_UNLOCK(&ia4->ia_ifa);
				break;
			}
			IFA_UNLOCK(&ia4->ia_ifa);
		}
		lck_rw_done(&in_ifaddr_rwlock);
		if (ia4 == NULL) {
			continue;
		}

		ifa_addref(ia);         /* for caller */
		ifnet_lock_done(ifp);
		return ifatoia6(ia);
	}
	ifnet_lock_done(ifp);

	return NULL;
}

int
stf_pre_output(
	struct ifnet    *ifp,
	__unused protocol_family_t  protocol_family,
	struct mbuf     **m0,
	const struct sockaddr   *dst,
	__unused void *route,
	__unused char *desk_linkaddr,
	__unused char *frame_type)
{
	mbuf_ref_t m = *m0;
	struct stf_softc *__single sc;
	const struct sockaddr_in6 *__single dst6;
	const struct in_addr *__single in4;
	u_int8_t tos;
	struct ip *__single ip;
	struct ip6_hdr *__single ip6;
	struct in6_ifaddr *__single ia6;
	struct sockaddr_in *__single dst4;
	struct ip_out_args ipoa;
	errno_t result = 0;

	bzero(&ipoa, sizeof(ipoa));
	ipoa.ipoa_boundif = IFSCOPE_NONE;
	ipoa.ipoa_flags = IPOAF_SELECT_SRCIF;
	ipoa.ipoa_sotc = SO_TC_UNSPEC;
	ipoa.ipoa_netsvctype = _NET_SERVICE_TYPE_UNSPEC;

	sc = ifnet_softc(ifp);
	dst6 = SIN6(dst);

	/* just in case */
	if ((ifnet_flags(ifp) & IFF_UP) == 0) {
		printf("stf: IFF_DOWN\n");
		return ENETDOWN;
	}

	/*
	 * If we don't have an ip4 address that match my inner ip6 address,
	 * we shouldn't generate output.  Without this check, we'll end up
	 * using wrong IPv4 source.
	 */
	ia6 = stf_getsrcifa6(ifp);
	if (ia6 == NULL) {
		return ENETDOWN;
	}

	if (mbuf_len(m) < sizeof(*ip6)) {
		m = m_pullup(m, sizeof(*ip6));
		if (!m) {
			*m0 = NULL; /* makes sure this won't be double freed */
			ifa_remref(&ia6->ia_ifa);
			return ENOBUFS;
		}
		*m0 = m;
	}
	ip6 = mtod(m, struct ip6_hdr *);
	tos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;

	/*
	 * Pickup the right outer dst addr from the list of candidates.
	 * ip6_dst has priority as it may be able to give us shorter IPv4 hops.
	 */
	if (IN6_IS_ADDR_6TO4(&ip6->ip6_dst)) {
		in4 = GET_V4(&ip6->ip6_dst);
	} else if (IN6_IS_ADDR_6TO4(&dst6->sin6_addr)) {
		in4 = GET_V4(&dst6->sin6_addr);
	} else {
		ifa_remref(&ia6->ia_ifa);
		return ENETUNREACH;
	}

	if (ifp->if_bpf) {
		/* We need to prepend the address family as a four byte field. */
		u_int32_t af = AF_INET6;

		bpf_tap_out(ifp, 0, m, &af, sizeof(af));
	}

	M_PREPEND(m, sizeof(struct ip), M_DONTWAIT, 1);
	if (m && mbuf_len(m) < sizeof(struct ip)) {
		m = m_pullup(m, sizeof(struct ip));
	}
	if (m == NULL) {
		*m0 = NULL;
		ifa_remref(&ia6->ia_ifa);
		return ENOBUFS;
	}

	*m0 = m;
	ip = mtod(m, struct ip *);

	bzero(ip, sizeof(*ip));

	IFA_LOCK_SPIN(&ia6->ia_ifa);
	bcopy(GET_V4(&(SIN6(&ia6->ia_addr))->sin6_addr),
	    &ip->ip_src, sizeof(ip->ip_src));
	IFA_UNLOCK(&ia6->ia_ifa);
	bcopy(in4, &ip->ip_dst, sizeof(ip->ip_dst));
	ip->ip_p = IPPROTO_IPV6;
	ip->ip_ttl = ip_stf_ttl;
	ip->ip_len = m->m_pkthdr.len;   /*host order*/
	if (ifp->if_flags & IFF_LINK1) {
		ip_ecn_ingress(ECN_NORMAL, &ip->ip_tos, &tos);
	} else {
		ip_ecn_ingress(ECN_NOCARE, &ip->ip_tos, &tos);
	}

	lck_mtx_lock(&sc->sc_ro_mtx);
	dst4 = SIN(&sc->sc_ro.ro_dst);
	if (ROUTE_UNUSABLE(&sc->sc_ro) || dst4->sin_family != AF_INET ||
	    bcmp(&dst4->sin_addr, &ip->ip_dst, sizeof(ip->ip_dst)) != 0) {
		ROUTE_RELEASE(&sc->sc_ro);
		/* cache route doesn't match: always the case during the first use */
		dst4->sin_family = AF_INET;
		dst4->sin_len = sizeof(struct sockaddr_in);
		bcopy(&ip->ip_dst, &dst4->sin_addr, sizeof(dst4->sin_addr));
	}

	result = ip_output(m, NULL, &sc->sc_ro, IP_OUTARGS, NULL, &ipoa);
	lck_mtx_unlock(&sc->sc_ro_mtx);

	/* Assumption: ip_output will free mbuf on errors */
	/* All the output processing is done here, don't let stf_output be called */
	if (result == 0) {
		result = EJUSTRETURN;
	}
	*m0 = NULL;
	ifa_remref(&ia6->ia_ifa);
	return result;
}
static errno_t
stf_output(
	__unused ifnet_t        ifp,
	__unused mbuf_t m)
{
	/* All processing is done in stf_pre_output
	 * this shouldn't be called as the pre_output returns "EJUSTRETURN"
	 */
	return 0;
}

static int
stf_checkaddr4(
	struct stf_softc *sc,
	const struct in_addr *in,
	struct ifnet *inifp)    /* incoming interface */
{
	struct in_ifaddr *__single ia4;

	/*
	 * reject packets with the following address:
	 * 224.0.0.0/4 0.0.0.0/8 127.0.0.0/8 255.0.0.0/8
	 */
	if (IN_MULTICAST(ntohl(in->s_addr))) {
		return -1;
	}
	switch ((ntohl(in->s_addr) & 0xff000000) >> 24) {
	case 0: case 127: case 255:
		return -1;
	}

	/*
	 * reject packets with broadcast
	 */
	lck_rw_lock_shared(&in_ifaddr_rwlock);
	for (ia4 = TAILQ_FIRST(&in_ifaddrhead);
	    ia4;
	    ia4 = TAILQ_NEXT(ia4, ia_link)) {
		IFA_LOCK(&ia4->ia_ifa);
		if ((ia4->ia_ifa.ifa_ifp->if_flags & IFF_BROADCAST) == 0) {
			IFA_UNLOCK(&ia4->ia_ifa);
			continue;
		}
		if (in->s_addr == ia4->ia_broadaddr.sin_addr.s_addr) {
			IFA_UNLOCK(&ia4->ia_ifa);
			lck_rw_done(&in_ifaddr_rwlock);
			return -1;
		}
		IFA_UNLOCK(&ia4->ia_ifa);
	}
	lck_rw_done(&in_ifaddr_rwlock);

	/*
	 * perform ingress filter
	 */
	if (sc && (ifnet_flags(sc->sc_if) & IFF_LINK2) == 0 && inifp) {
		struct sockaddr_in sin;
		rtentry_ref_t rt;

		SOCKADDR_ZERO(&sin, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(struct sockaddr_in);
		sin.sin_addr = *in;
		rt = rtalloc1(SA(&sin), 0, 0);
		if (rt != NULL) {
			RT_LOCK(rt);
		}
		if (rt == NULL || rt->rt_ifp != inifp) {
#if 1
			log(LOG_WARNING, "%s: packet from 0x%x dropped "
			    "due to ingress filter\n", if_name(sc->sc_if),
			    (u_int32_t)ntohl(sin.sin_addr.s_addr));
#endif
			if (rt != NULL) {
				RT_UNLOCK(rt);
				rtfree(rt);
			}
			return -1;
		}
		RT_UNLOCK(rt);
		rtfree(rt);
	}

	return 0;
}

static int
stf_checkaddr6(
	struct stf_softc *sc,
	struct in6_addr *in6,
	struct ifnet *inifp)    /* incoming interface */
{
	/*
	 * check 6to4 addresses
	 */
	if (IN6_IS_ADDR_6TO4(in6)) {
		return stf_checkaddr4(sc, GET_V4(in6), inifp);
	}

	/*
	 * reject anything that look suspicious.  the test is implemented
	 * in ip6_input too, but we check here as well to
	 * (1) reject bad packets earlier, and
	 * (2) to be safe against future ip6_input change.
	 */
	if (IN6_IS_ADDR_V4COMPAT(in6) || IN6_IS_ADDR_V4MAPPED(in6)) {
		return -1;
	}

	return 0;
}

static void
in_stf_input(
	struct mbuf *m,
	int off)
{
	struct stf_softc *__single sc;
	struct ip *__single ip;
	struct ip6_hdr ip6;
	u_int8_t otos, itos;
	int proto;
	struct ifnet *__single ifp;
	struct ifnet_stat_increment_param stats;

	ip = mtod(m, struct ip *);
	proto = ip->ip_p;

	if (proto != IPPROTO_IPV6) {
		m_freem(m);
		return;
	}

	ip = mtod(m, struct ip *);

	sc = (struct stf_softc *)encap_getarg(m);

	if (sc == NULL || (ifnet_flags(sc->sc_if) & IFF_UP) == 0) {
		m_freem(m);
		return;
	}

	ifp = sc->sc_if;

	/*
	 * perform sanity check against outer src/dst.
	 * for source, perform ingress filter as well.
	 */
	if (stf_checkaddr4(sc, &ip->ip_dst, NULL) < 0 ||
	    stf_checkaddr4(sc, &ip->ip_src, m->m_pkthdr.rcvif) < 0) {
		m_freem(m);
		return;
	}

	otos = ip->ip_tos;
	if (mbuf_copydata(m, off, sizeof(ip6), &ip6)) {
		m_freem(m);
		return;
	}

	/*
	 * perform sanity check against inner src/dst.
	 * for source, perform ingress filter as well.
	 */
	if (stf_checkaddr6(sc, &ip6.ip6_dst, NULL) < 0 ||
	    stf_checkaddr6(sc, &ip6.ip6_src, m->m_pkthdr.rcvif) < 0) {
		m_freem(m);
		return;
	}

	itos = (ntohl(ip6.ip6_flow) >> 20) & 0xff;
	if ((ifnet_flags(ifp) & IFF_LINK1) != 0) {
		ip_ecn_egress(ECN_NORMAL, &otos, &itos);
	} else {
		ip_ecn_egress(ECN_NOCARE, &otos, &itos);
	}
	ip6.ip6_flow &= ~htonl(0xff << 20);
	ip6.ip6_flow |= htonl((u_int32_t)itos << 20);

	m->m_pkthdr.rcvif = ifp;
	mbuf_pkthdr_setheader(m, mtod(m, void*));
	mbuf_adj(m, off);

	if (ifp->if_bpf) {
		/* We need to prepend the address family as a four byte field. */
		u_int32_t af = AF_INET6;
		bpf_tap_in(ifp, 0, m, &af, sizeof(af));
	}

	/*
	 * Put the packet to the network layer input queue according to the
	 * specified address family.
	 * See net/if_gif.c for possible issues with packet processing
	 * reorder due to extra queueing.
	 */
	bzero(&stats, sizeof(stats));
	stats.packets_in = 1;
	stats.bytes_in = mbuf_pkthdr_len(m);
	mbuf_pkthdr_setrcvif(m, ifp);
	ifnet_input(ifp, m, &stats);

	return;
}

static void
stf_rtrequest(
	__unused int cmd,
	struct rtentry *rt,
	__unused struct sockaddr *sa)
{
	if (rt != NULL) {
		RT_LOCK_ASSERT_HELD(rt);
		rt->rt_rmx.rmx_mtu = IPV6_MMTU;
	}
}

static errno_t
stf_ioctl(
	ifnet_t         ifp,
	u_long          cmd,
	void            *__sized_by(IOCPARM_LEN(cmd)) data)
{
	struct ifaddr *__single ifa;
	struct ifreq *__single ifr;
	struct sockaddr_in6 *__single sin6;
	int error;

	error = 0;
	switch (cmd) {
	case SIOCSIFADDR:
		ifa = (struct ifaddr *)data;
		if (ifa == NULL) {
			error = EAFNOSUPPORT;
			break;
		}
		IFA_LOCK(ifa);
		if (ifa->ifa_addr->sa_family != AF_INET6) {
			IFA_UNLOCK(ifa);
			error = EAFNOSUPPORT;
			break;
		}
		sin6 = SIN6(ifa->ifa_addr);
		if (IN6_IS_ADDR_6TO4(&sin6->sin6_addr)) {
			if (!(ifnet_flags( ifp ) & IFF_UP)) {
				/* do this only if the interface is not already up */
				ifa->ifa_rtrequest = stf_rtrequest;
				IFA_UNLOCK(ifa);
				ifnet_set_flags(ifp, IFF_UP, IFF_UP);
			} else {
				IFA_UNLOCK(ifa);
			}
		} else {
			IFA_UNLOCK(ifa);
			error = EINVAL;
		}
		IFA_LOCK_ASSERT_NOTHELD(ifa);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ifr = (struct ifreq *)data;
		if (ifr && ifr->ifr_addr.sa_family == AF_INET6) {
			;
		} else {
			error = EAFNOSUPPORT;
		}
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	return error;
}
