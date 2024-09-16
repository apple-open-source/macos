/*
 * Copyright (c) 2000-2024 Apple Inc. All rights reserved.
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
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 *
 * $FreeBSD: src/sys/netinet6/raw_ip6.c,v 1.7.2.4 2001/07/29 19:32:40 ume Exp $
 */

/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)raw_ip.c	8.2 (Berkeley) 1/4/94
 */
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/mcache.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/net_api_stats.h>
#include <net/route.h>
#include <net/if_types.h>
#include <net/content_filter.h>
#include <net/droptap.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/in_tclass.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet/in_pcb.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/nd6.h>
#include <netinet6/ip6protosw.h>
#include <netinet6/scope6_var.h>
#include <netinet6/raw_ip6.h>

#if IPSEC
#include <netinet6/ipsec.h>
#include <netinet6/ipsec6.h>
#endif /*IPSEC*/

#if NECP
#include <net/necp.h>
#endif

/*
 * Raw interface to IP6 protocol.
 */

extern struct   inpcbhead ripcb;
extern struct   inpcbinfo ripcbinfo;
extern u_int32_t        rip_sendspace;
extern u_int32_t        rip_recvspace;

struct rip6stat rip6stat;

/*
 * Setup generic address and protocol structures
 * for raw_input routine, then pass them along with
 * mbuf chain.
 */
int
rip6_input(
	struct  mbuf **mp,
	int     *offp,
	int     proto)
{
	struct mbuf *__single m = *mp;
	struct ip6_hdr *__single ip6 = mtod(m, struct ip6_hdr *);
	struct inpcb *__single in6p;
	struct inpcb *__single last = 0;
	struct mbuf *__single opts = NULL;
	struct sockaddr_in6 rip6src;
	int ret;
	struct ifnet *__single ifp = m->m_pkthdr.rcvif;
	boolean_t is_wake_pkt = false;

	/* Expect 32-bit aligned data pointer on strict-align platforms */
	MBUF_STRICT_DATA_ALIGNMENT_CHECK_32(m);

	rip6stat.rip6s_ipackets++;

	init_sin6(&rip6src, m); /* general init */

	if ((m->m_flags & M_PKTHDR) && (m->m_pkthdr.pkt_flags & PKTF_WAKE_PKT)) {
		is_wake_pkt = true;
	}

	lck_rw_lock_shared(&ripcbinfo.ipi_lock);
	LIST_FOREACH(in6p, &ripcb, inp_list) {
		if ((in6p->in6p_vflag & INP_IPV6) == 0) {
			continue;
		}
		if (in6p->in6p_ip6_nxt &&
		    in6p->in6p_ip6_nxt != proto) {
			continue;
		}
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr) &&
		    !in6_are_addr_equal_scoped(&in6p->in6p_laddr, &ip6->ip6_dst, in6p->inp_lifscope, ifp->if_index)) {
			continue;
		}
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr) &&
		    !in6_are_addr_equal_scoped(&in6p->in6p_faddr, &ip6->ip6_src, in6p->inp_fifscope, ifp->if_index)) {
			continue;
		}

		if (inp_restricted_recv(in6p, ifp)) {
			continue;
		}

		if (proto == IPPROTO_ICMPV6 || in6p->in6p_cksum != -1) {
			rip6stat.rip6s_isum++;
			if (in6_cksum(m, ip6->ip6_nxt, *offp,
			    m->m_pkthdr.len - *offp)) {
				rip6stat.rip6s_badsum++;
				continue;
			}
		}
		if (last) {
			struct mbuf *n = m_copy(m, 0, (int)M_COPYALL);

#if NECP
			if (n && !necp_socket_is_allowed_to_send_recv_v6(in6p, 0, 0,
			    &ip6->ip6_dst, &ip6->ip6_src, ifp, 0, NULL, NULL, NULL, NULL)) {
				m_drop(n, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_NECP_POLICY_DROP, NULL, 0);
				/* do not inject data into pcb */
			} else
#endif /* NECP */
			if (n) {
				if ((last->in6p_flags & INP_CONTROLOPTS) != 0 ||
				    SOFLOW_ENABLED(last->in6p_socket) ||
				    SO_RECV_CONTROL_OPTS(last->in6p_socket)) {
					ret = ip6_savecontrol(last, n, &opts);
					if (ret != 0) {
						m_drop(n, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_ENOBUFS, NULL, 0);
						m_freem(opts);
						opts = NULL;
						last = in6p;
						continue;
					}
				}
				/* strip intermediate headers */
				m_adj(n, *offp);
				so_recv_data_stat(last->in6p_socket, m, 0);
				if (sbappendaddr(&last->in6p_socket->so_rcv,
				    (struct sockaddr *)&rip6src,
				    n, opts, NULL) == 0) {
					rip6stat.rip6s_fullsock++;
				} else {
					sorwakeup(last->in6p_socket);
				}
				if (is_wake_pkt) {
					soevent(in6p->in6p_socket,
					    SO_FILT_HINT_LOCKED | SO_FILT_HINT_WAKE_PKT);
				}
				opts = NULL;
			}
		}
		last = in6p;
	}

#if NECP
	if (last && !necp_socket_is_allowed_to_send_recv_v6(in6p, 0, 0,
	    &ip6->ip6_dst, &ip6->ip6_src, ifp, 0, NULL, NULL, NULL, NULL)) {
		m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_NECP_POLICY_DROP, NULL, 0);
		ip6stat.ip6s_delivered--;
		/* do not inject data into pcb */
	} else
#endif /* NECP */
	if (last) {
		if ((last->in6p_flags & INP_CONTROLOPTS) != 0 ||
		    SOFLOW_ENABLED(last->in6p_socket) ||
		    SO_RECV_CONTROL_OPTS(last->in6p_socket)) {
			ret = ip6_savecontrol(last, m, &opts);
			if (ret != 0) {
				m_freem(m);
				m_freem(opts);
				ip6stat.ip6s_delivered--;
				goto unlock;
			}
		}
		/* strip intermediate headers */
		m_adj(m, *offp);
		so_recv_data_stat(last->in6p_socket, m, 0);
		if (sbappendaddr(&last->in6p_socket->so_rcv,
		    (struct sockaddr *)&rip6src, m, opts, NULL) == 0) {
			rip6stat.rip6s_fullsock++;
		} else {
			sorwakeup(last->in6p_socket);
		}
		if (is_wake_pkt) {
			soevent(last->in6p_socket,
			    SO_FILT_HINT_LOCKED | SO_FILT_HINT_WAKE_PKT);
		}
	} else {
		rip6stat.rip6s_nosock++;
		if (m->m_flags & M_MCAST) {
			rip6stat.rip6s_nosockmcast++;
		}
		if (proto == IPPROTO_NONE) {
			m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_NO_SOCK, NULL, 0);
		} else {
			char *prvnxtp = ip6_get_prevhdr(m, *offp); /* XXX */
			icmp6_error(m, ICMP6_PARAM_PROB,
			    ICMP6_PARAMPROB_NEXTHEADER,
			    (int)(prvnxtp - mtod(m, char *)));
		}
		ip6stat.ip6s_delivered--;
	}

unlock:
	lck_rw_done(&ripcbinfo.ipi_lock);

	return IPPROTO_DONE;
}

void
rip6_ctlinput(
	int cmd,
	struct sockaddr *sa,
	void *d,
	__unused struct ifnet *ifp)
{
	struct ip6_hdr *__single ip6 = NULL;
	struct mbuf *__single m = NULL;
	void *__single cmdarg = NULL;
	int off = 0;
	struct ip6ctlparam *__single ip6cp = NULL;
	const struct sockaddr_in6 *__single sa6_src = NULL;
	void (*notify)(struct inpcb *, int) = in6_rtchange;

	if (sa->sa_family != AF_INET6 ||
	    sa->sa_len != sizeof(struct sockaddr_in6)) {
		return;
	}

	if ((unsigned)cmd >= PRC_NCMDS) {
		return;
	}
	if (PRC_IS_REDIRECT(cmd)) {
		notify = in6_rtchange;
		d = NULL;
	} else if (cmd == PRC_HOSTDEAD) {
		d = NULL;
	} else if (inet6ctlerrmap[cmd] == 0) {
		return;
	}

	/* if the parameter is from icmp6, decode it. */
	if (d != NULL) {
		ip6cp = (struct ip6ctlparam *)d;
		m = ip6cp->ip6c_m;
		ip6 = ip6cp->ip6c_ip6;
		off = ip6cp->ip6c_off;
		cmdarg = ip6cp->ip6c_cmdarg;
		sa6_src = ip6cp->ip6c_src;
	} else {
		m = NULL;
		ip6 = NULL;
		cmdarg = NULL;
		sa6_src = &sa6_any;
	}

	(void) in6_pcbnotify(&ripcbinfo, sa, 0, (const struct sockaddr *)sa6_src,
	    0, cmd, cmdarg, notify);
}

/*
 * Generate IPv6 header and pass packet to ip6_output.
 * Tack on options user may have setup with control call.
 */
int
rip6_output(
	struct mbuf *m,
	struct socket *so,
	struct sockaddr_in6 *dstsock,
	struct mbuf *control,
	int israw)
{
	struct in6_addr *__single dst;
	struct ip6_hdr *__single ip6;
	struct inpcb *__single in6p;
	u_int   plen = m->m_pkthdr.len;
	int error = 0;
	struct ip6_pktopts opt, *__single optp = NULL;
	struct ip6_moptions *__single im6o = NULL;
	struct ifnet *__single oifp = NULL;
	int type = 0, code = 0;         /* for ICMPv6 output statistics only */
	int sotc = SO_TC_UNSPEC;
	int netsvctype = _NET_SERVICE_TYPE_UNSPEC;
	struct ip6_out_args ip6oa;
	int flags = IPV6_OUTARGS;
	struct sockaddr_in6 tmp;
#if CONTENT_FILTER
	struct m_tag *cfil_tag = NULL;
	bool cfil_faddr_use = false;
	uint32_t cfil_so_state_change_cnt = 0;
	uint32_t cfil_so_options = 0;
	uint32_t sifscope = IFSCOPE_NONE, difscope = IFSCOPE_NONE;
	struct sockaddr *__single cfil_faddr = NULL;
	struct sockaddr_in6 *__single cfil_sin6 = NULL;
#endif
	drop_reason_t drop_reason = DROP_REASON_UNSPECIFIED;

	in6p = sotoin6pcb(so);
	if (in6p == NULL) {
		error = EINVAL;
		goto bad;
	}

#if CONTENT_FILTER
	/*
	 * If socket is subject to Content Filter and no addr is passed in,
	 * retrieve CFIL saved state from mbuf and use it if necessary.
	 */
	if (CFIL_DGRAM_FILTERED(so) && !dstsock) {
		cfil_tag = cfil_dgram_get_socket_state(m, &cfil_so_state_change_cnt, &cfil_so_options, &cfil_faddr, NULL);
		if (cfil_tag) {
			cfil_sin6 = SIN6(cfil_faddr);
			if (IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr)) {
				/*
				 * Socket is unconnected, simply use the saved faddr as 'addr' to go through
				 * the connect/disconnect logic.
				 */
				dstsock = cfil_sin6;
			} else if ((so->so_state_change_cnt != cfil_so_state_change_cnt) &&
			    (in6p->in6p_fport != cfil_sin6->sin6_port ||
			    !in6_are_addr_equal_scoped(&in6p->in6p_faddr, &cfil_sin6->sin6_addr, in6p->inp_fifscope, cfil_sin6->sin6_scope_id))) {
				/*
				 * Socket is connected but socket state and dest addr/port changed.
				 * We need to use the saved faddr and socket options.
				 */
				cfil_faddr_use = true;
			}
		}
	}
#endif

	/* always copy sockaddr to avoid overwrites */
	if (so->so_state & SS_ISCONNECTED) {
		if (dstsock != NULL) {
			error = EISCONN;
			drop_reason = DROP_REASON_IP_EISCONN;
			goto bad;
		}
		/* XXX */
		bzero(&tmp, sizeof(tmp));
		tmp.sin6_family = AF_INET6;
		tmp.sin6_len = sizeof(struct sockaddr_in6);
		bcopy(
#if CONTENT_FILTER
			cfil_faddr_use ? &cfil_sin6->sin6_addr :
#endif
			&in6p->in6p_faddr, &tmp.sin6_addr, sizeof(struct in6_addr));
		dstsock = &tmp;
	} else {
		if (dstsock == NULL) {
			error = ENOTCONN;
			goto bad;
		}
		tmp = *dstsock;
		dstsock = &tmp;
	}

#if ENABLE_DEFAULT_SCOPE
	if (dstsock->sin6_scope_id == 0) { /* not change if specified  */
		dstsock->sin6_scope_id = scope6_addr2default(&dstsock->sin6_addr);
	}
#endif

	bzero(&ip6oa, sizeof(ip6oa));
	ip6oa.ip6oa_boundif = IFSCOPE_NONE;
	ip6oa.ip6oa_flags = IP6OAF_SELECT_SRCIF;

	if (in6p == NULL
#if NECP
	    || (necp_socket_should_use_flow_divert(in6p))
#endif /* NECP */
	    ) {
		if (in6p == NULL) {
			error = EINVAL;
		} else {
			error = EPROTOTYPE;
		}
		goto bad;
	}
	if (dstsock != NULL && IN6_IS_ADDR_V4MAPPED(&dstsock->sin6_addr)) {
		error = EINVAL;
		goto bad;
	}

	if (in6p->inp_flags & INP_BOUND_IF) {
		ip6oa.ip6oa_boundif = in6p->inp_boundifp->if_index;
		ip6oa.ip6oa_flags |= IP6OAF_BOUND_IF;
	} else if (!in6_embedded_scope && IN6_IS_SCOPE_EMBED(&in6p->in6p_faddr)) {
		ip6oa.ip6oa_boundif = dstsock->sin6_scope_id;
		ip6oa.ip6oa_flags |= IP6OAF_BOUND_IF;
	}
	if (INP_NO_CELLULAR(in6p)) {
		ip6oa.ip6oa_flags |= IP6OAF_NO_CELLULAR;
	}
	if (INP_NO_EXPENSIVE(in6p)) {
		ip6oa.ip6oa_flags |= IP6OAF_NO_EXPENSIVE;
	}
	if (INP_NO_CONSTRAINED(in6p)) {
		ip6oa.ip6oa_flags |= IP6OAF_NO_CONSTRAINED;
	}
	if (INP_AWDL_UNRESTRICTED(in6p)) {
		ip6oa.ip6oa_flags |= IP6OAF_AWDL_UNRESTRICTED;
	}
	if (INP_INTCOPROC_ALLOWED(in6p)) {
		ip6oa.ip6oa_flags |= IP6OAF_INTCOPROC_ALLOWED;
	}
	if (INP_MANAGEMENT_ALLOWED(in6p)) {
		ip6oa.ip6oa_flags |= IP6OAF_MANAGEMENT_ALLOWED;
	}

	dst = &dstsock->sin6_addr;
	if (control) {
		sotc = so_tc_from_control(control, &netsvctype);

		if ((error = ip6_setpktopts(control, &opt,
		    in6p->in6p_outputopts, SOCK_PROTO(so))) != 0) {
			goto bad;
		}
		optp = &opt;
	} else {
		optp = in6p->in6p_outputopts;
	}
	if (sotc == SO_TC_UNSPEC) {
		sotc = so->so_traffic_class;
		netsvctype = so->so_netsvctype;
	}
	ip6oa.ip6oa_sotc = sotc;
	ip6oa.ip6oa_netsvctype = netsvctype;

	/*
	 * For an ICMPv6 packet, we should know its type and code
	 * to update statistics.
	 */
	if (SOCK_PROTO(so) == IPPROTO_ICMPV6) {
		struct icmp6_hdr *icmp6;
		if (m->m_len < sizeof(struct icmp6_hdr) &&
		    (m = m_pullup(m, sizeof(struct icmp6_hdr))) == NULL) {
			error = ENOBUFS;
			goto bad;
		}
		icmp6 = mtod(m, struct icmp6_hdr *);
		type = icmp6->icmp6_type;
		code = icmp6->icmp6_code;
	}

	if (in6p->inp_flowhash == 0) {
		inp_calc_flowhash(in6p);
		ASSERT(in6p->inp_flowhash != 0);
	}
	/* update flowinfo - RFC 6437 */
	if (in6p->inp_flow == 0 && in6p->in6p_flags & IN6P_AUTOFLOWLABEL) {
		in6p->inp_flow &= ~IPV6_FLOWLABEL_MASK;
		in6p->inp_flow |=
		    (htonl(ip6_randomflowlabel()) & IPV6_FLOWLABEL_MASK);
	}

	M_PREPEND(m, sizeof(*ip6), M_WAIT, 1);
	if (m == NULL) {
		error = ENOBUFS;
		goto bad;
	}
	ip6 = mtod(m, struct ip6_hdr *);

	/*
	 * Next header might not be ICMP6 but use its pseudo header anyway.
	 */
	ip6->ip6_dst = *dst;

	im6o = in6p->in6p_moptions;

	/*
	 * If the scope of the destination is link-local, embed the interface
	 * index in the address.
	 *
	 * XXX advanced-api value overrides sin6_scope_id
	 */
	if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_dst)) {
		struct in6_pktinfo *pi;
		struct ifnet *im6o_multicast_ifp = NULL;

		if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) && im6o != NULL) {
			IM6O_LOCK(im6o);
			im6o_multicast_ifp = im6o->im6o_multicast_ifp;
			IM6O_UNLOCK(im6o);
		}
		/*
		 * XXX Boundary check is assumed to be already done in
		 * ip6_setpktoptions().
		 */
		ifnet_head_lock_shared();
		if (optp && (pi = optp->ip6po_pktinfo) && pi->ipi6_ifindex) {
			if (in6_embedded_scope) {
				ip6->ip6_dst.s6_addr16[1] = htons((uint16_t)pi->ipi6_ifindex);
			}
			oifp = ifindex2ifnet[pi->ipi6_ifindex];
			difscope = pi->ipi6_ifindex;
			if (oifp != NULL) {
				ifnet_reference(oifp);
			}
		} else if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) &&
		    im6o != NULL && im6o_multicast_ifp != NULL) {
			oifp = im6o_multicast_ifp;
			ifnet_reference(oifp);
			if (in6_embedded_scope) {
				ip6->ip6_dst.s6_addr16[1] = htons(oifp->if_index);
			}
			difscope = oifp->if_index;
		} else if (dstsock->sin6_scope_id) {
			/*
			 * boundary check
			 *
			 * Sinced dstsock->sin6_scope_id is unsigned, we don't
			 * need to check if it's < 0
			 */
			if (!IF_INDEX_IN_RANGE(dstsock->sin6_scope_id)) {
				error = ENXIO;  /* XXX EINVAL? */
				drop_reason = DROP_REASON_IP6_BAD_SCOPE;
				ifnet_head_done();
				goto bad;
			}
			if (in6_embedded_scope) {
				ip6->ip6_dst.s6_addr16[1]
				        = htons(dstsock->sin6_scope_id & 0xffff);        /*XXX*/
			}
			difscope = dstsock->sin6_scope_id;
		}
		ifnet_head_done();

		ip6_output_setdstifscope(m, difscope, NULL);
	}

	/*
	 * Source address selection.
	 */
	{
		struct in6_addr *__single in6a;
		struct in6_addr storage;
		u_short index = 0;

		if (israw != 0 && optp && optp->ip6po_pktinfo && !IN6_IS_ADDR_UNSPECIFIED(&optp->ip6po_pktinfo->ipi6_addr)) {
			in6a = &optp->ip6po_pktinfo->ipi6_addr;
			flags |= IPV6_FLAG_NOSRCIFSEL;
			sifscope = optp->ip6po_pktinfo->ipi6_ifindex;
		} else {
			struct ifnet *__single src_ifp = NULL;
			in6a = in6_selectsrc(dstsock, optp, in6p,
			    &in6p->in6p_route, &src_ifp, &storage, ip6oa.ip6oa_boundif,
			    &error);
			if (src_ifp != NULL) {
				in6p->inp_lifscope  = src_ifp->if_index;
				ifnet_release(src_ifp);
			} else {
				in6p->inp_lifscope = ip6oa.ip6oa_boundif;
			}
			if (in6a != 0) {
				ip6oa.ip6oa_flags |= IP6OAF_BOUND_SRCADDR;
			} else {
				if (error == 0) {
					error = EADDRNOTAVAIL;
				}
				goto bad;
			}
		}

		ip6->ip6_src = *in6a;
		if (IN6_IS_SCOPE_EMBED(in6a) && sifscope == IFSCOPE_NONE) {
			sifscope = difscope;
		}
		ip6_output_setsrcifscope(m, sifscope, NULL);

		if (in6p->in6p_route.ro_rt != NULL) {
			RT_LOCK(in6p->in6p_route.ro_rt);
			if (in6p->in6p_route.ro_rt->rt_ifp != NULL) {
				index = in6p->in6p_route.ro_rt->rt_ifp->if_index;
			}
			RT_UNLOCK(in6p->in6p_route.ro_rt);
			if (oifp != NULL) {
				ifnet_release(oifp);
			}
			ifnet_head_lock_shared();
			if (!IF_INDEX_IN_RANGE(index)) {
				panic("bad if_index on interface from route");
			}
			oifp = ifindex2ifnet[index];
			if (oifp != NULL) {
				ifnet_reference(oifp);
			}
			ifnet_head_done();
		}
	}
	ip6->ip6_flow = (ip6->ip6_flow & ~IPV6_FLOWINFO_MASK) |
	    (in6p->inp_flow & IPV6_FLOWINFO_MASK);
	ip6->ip6_vfc = (ip6->ip6_vfc & ~IPV6_VERSION_MASK) |
	    (IPV6_VERSION & IPV6_VERSION_MASK);
	/* ip6_plen will be filled in ip6_output, so not fill it here. */
	ip6->ip6_nxt = in6p->in6p_ip6_nxt;
	ip6->ip6_hlim = in6_selecthlim(in6p, oifp);

	if (SOCK_PROTO(so) == IPPROTO_ICMPV6 || in6p->in6p_cksum != -1) {
		struct mbuf *n;
		int off;
		u_int16_t *p;

		/* compute checksum */
		if (SOCK_PROTO(so) == IPPROTO_ICMPV6) {
			off = offsetof(struct icmp6_hdr, icmp6_cksum);
		} else {
			off = in6p->in6p_cksum;
		}
		if (plen < (unsigned int)(off + 1)) {
			error = EINVAL;
			goto bad;
		}
		off += sizeof(struct ip6_hdr);

		n = m;
		while (n && n->m_len <= off) {
			off -= n->m_len;
			n = n->m_next;
		}
		if (!n) {
			goto bad;
		}
		p = (u_int16_t *)(void *)(mtod(n, caddr_t) + off);
		*p = 0;
		*p = in6_cksum(m, ip6->ip6_nxt, sizeof(*ip6), plen);
	}

#if NECP
	{
		necp_kernel_policy_id policy_id;
		necp_kernel_policy_id skip_policy_id;
		u_int32_t route_rule_id;
		u_int32_t pass_flags;

		/*
		 * We need a route to perform NECP route rule checks
		 */
		if ((net_qos_policy_restricted != 0 &&
		    ROUTE_UNUSABLE(&in6p->in6p_route))
#if CONTENT_FILTER
		    || cfil_faddr_use
#endif
		    ) {
			struct sockaddr_in6 to;
			struct sockaddr_in6 from;

			ROUTE_RELEASE(&in6p->in6p_route);

			bzero(&from, sizeof(struct sockaddr_in6));
			from.sin6_family = AF_INET6;
			from.sin6_len = sizeof(struct sockaddr_in6);
			from.sin6_addr = ip6->ip6_src;

			bzero(&to, sizeof(struct sockaddr_in6));
			to.sin6_family = AF_INET6;
			to.sin6_len = sizeof(struct sockaddr_in6);
			to.sin6_addr = ip6->ip6_dst;

			in6p->in6p_route.ro_dst.sin6_family = AF_INET6;
			in6p->in6p_route.ro_dst.sin6_len = sizeof(struct sockaddr_in6);
			((struct sockaddr_in6 *)(void *)&in6p->in6p_route.ro_dst)->sin6_addr =
			    ip6->ip6_dst;

			rtalloc_scoped((struct route *)&in6p->in6p_route, ip6oa.ip6oa_boundif);

			inp_update_necp_policy(in6p, (struct sockaddr *)&from,
			    (struct sockaddr *)&to, ip6oa.ip6oa_boundif);
			in6p->inp_policyresult.results.qos_marking_gencount = 0;
		}

		if (!necp_socket_is_allowed_to_send_recv_v6(in6p, 0, 0,
		    &ip6->ip6_src, &ip6->ip6_dst, NULL, 0, &policy_id, &route_rule_id, &skip_policy_id, &pass_flags)) {
			error = EHOSTUNREACH;
			drop_reason = DROP_REASON_IP_NECP_POLICY_DROP;
			goto bad;
		}

		necp_mark_packet_from_socket(m, in6p, policy_id, route_rule_id, skip_policy_id, pass_flags);

		if (net_qos_policy_restricted != 0) {
			necp_socket_update_qos_marking(in6p, in6p->in6p_route.ro_rt, route_rule_id);
		}
	}
#endif /* NECP */
	if ((so->so_flags1 & SOF1_QOSMARKING_ALLOWED)) {
		ip6oa.ip6oa_flags |= IP6OAF_QOSMARKING_ALLOWED;
	}

#if IPSEC
	if (in6p->in6p_sp != NULL && ipsec_setsocket(m, so) != 0) {
		error = ENOBUFS;
		drop_reason = DROP_REASON_IP_ENOBUFS;
		goto bad;
	}
#endif /*IPSEC*/

	if (ROUTE_UNUSABLE(&in6p->in6p_route)) {
		ROUTE_RELEASE(&in6p->in6p_route);
	}

	if (oifp != NULL) {
		ifnet_release(oifp);
		oifp = NULL;
	}

	set_packet_service_class(m, so, sotc, PKT_SCF_IPV6);
	m->m_pkthdr.pkt_flowsrc = FLOWSRC_INPCB;
	m->m_pkthdr.pkt_flowid = in6p->inp_flowhash;
	m->m_pkthdr.pkt_flags |= (PKTF_FLOW_ID | PKTF_FLOW_LOCALSRC |
	    PKTF_FLOW_RAWSOCK);
	m->m_pkthdr.pkt_proto = in6p->in6p_ip6_nxt;
	m->m_pkthdr.tx_rawip_pid = so->last_pid;
	if (so->so_flags & SOF_DELEGATED) {
		m->m_pkthdr.tx_rawip_e_pid = so->e_pid;
	} else {
		m->m_pkthdr.tx_rawip_e_pid = 0;
	}
#if (DEBUG || DEVELOPMENT)
	if (so->so_flags & SOF_MARK_WAKE_PKT) {
		so->so_flags &= ~SOF_MARK_WAKE_PKT;
		m->m_pkthdr.pkt_flags |= PKTF_WAKE_PKT;
	}
#endif /* (DEBUG || DEVELOPMENT) */

	if (im6o != NULL) {
		IM6O_ADDREF(im6o);
	}

	error = ip6_output(m, optp, &in6p->in6p_route, flags, im6o,
	    &oifp, &ip6oa);

	if (im6o != NULL) {
		IM6O_REMREF(im6o);
	}

	if (in6p->in6p_route.ro_rt != NULL) {
		struct rtentry *rt = in6p->in6p_route.ro_rt;
		struct ifnet *outif;

		if ((rt->rt_flags & RTF_MULTICAST) ||
		    in6p->in6p_socket == NULL ||
#if CONTENT_FILTER
		    /* Discard temporary route for cfil case */
		    cfil_faddr_use ||
#endif
		    !(in6p->in6p_socket->so_state & SS_ISCONNECTED)) {
			rt = NULL;      /* unusable */
		}
		/*
		 * Always discard the cached route for unconnected
		 * socket or if it is a multicast route.
		 */
		if (rt == NULL) {
			ROUTE_RELEASE(&in6p->in6p_route);
		}

		/*
		 * If this is a connected socket and the destination
		 * route is not multicast, update outif with that of
		 * the route interface index used by IP.
		 */
		if (rt != NULL) {
			/*
			 * When an NECP IP tunnel policy forces the outbound interface,
			 * ip6_output_list() informs the transport layer what is the actual
			 * outgoing interface
			 */
			if (ip6oa.ip6oa_flags & IP6OAF_BOUND_IF) {
				outif = ifindex2ifnet[ip6oa.ip6oa_boundif];
			} else {
				outif = rt->rt_ifp;
			}
			if (outif != NULL) {
				in6p->in6p_last_outifp = outif;
			}
		}
	} else {
		ROUTE_RELEASE(&in6p->in6p_route);
	}

	/*
	 * If output interface was cellular/expensive, and this socket is
	 * denied access to it, generate an event.
	 */
	if (error != 0 && (ip6oa.ip6oa_flags & IP6OAF_R_IFDENIED) &&
	    (INP_NO_CELLULAR(in6p) || INP_NO_EXPENSIVE(in6p) || INP_NO_CONSTRAINED(in6p))) {
		soevent(in6p->inp_socket, (SO_FILT_HINT_LOCKED |
		    SO_FILT_HINT_IFDENIED));
	}

	if (SOCK_PROTO(so) == IPPROTO_ICMPV6) {
		if (oifp) {
			icmp6_ifoutstat_inc(oifp, type, code);
		}
		icmp6stat.icp6s_outhist[type]++;
	} else {
		rip6stat.rip6s_opackets++;
	}

	goto freectl;

bad:
	if (m != NULL) {
		m_drop(m, DROPTAP_FLAG_DIR_OUT | DROPTAP_FLAG_L2_MISSING, drop_reason, NULL, 0);
	}

freectl:
	if (optp == &opt && optp->ip6po_rthdr) {
		ROUTE_RELEASE(&optp->ip6po_route);
	}

	if (control != NULL) {
		if (optp == &opt) {
			ip6_clearpktopts(optp, -1);
		}
		m_freem(control);
	}
	if (oifp != NULL) {
		ifnet_release(oifp);
	}
#if CONTENT_FILTER
	if (cfil_tag) {
		m_tag_free(cfil_tag);
	}
#endif

	return error;
}

/*
 * Raw IPv6 socket option processing.
 */
int
rip6_ctloutput(
	struct socket *so,
	struct sockopt *sopt)
{
	int error, optval;

	/* Allow <SOL_SOCKET,SO_FLUSH/SO_BINDTODEVICE> at this level */
	if (sopt->sopt_level == SOL_SOCKET) {
		if (sopt->sopt_name == SO_BINDTODEVICE) {
			return ip6_ctloutput(so, sopt);
		} else if (sopt->sopt_name != SO_FLUSH) {
			return EINVAL;
		}
	} else if (sopt->sopt_level == IPPROTO_ICMPV6) {
		/*
		 * XXX: is it better to call icmp6_ctloutput() directly
		 * from protosw?
		 */
		return icmp6_ctloutput(so, sopt);
	} else if (sopt->sopt_level != IPPROTO_IPV6) {
		return EINVAL;
	}

	error = 0;

	switch (sopt->sopt_dir) {
	case SOPT_GET:
		switch (sopt->sopt_name) {
		case IPV6_CHECKSUM:
			error = ip6_raw_ctloutput(so, sopt);
			break;
		default:
			error = ip6_ctloutput(so, sopt);
			break;
		}
		break;

	case SOPT_SET:
		switch (sopt->sopt_name) {
		case IPV6_CHECKSUM:
			error = ip6_raw_ctloutput(so, sopt);
			break;

		case SO_FLUSH:
			if ((error = sooptcopyin(sopt, &optval, sizeof(optval),
			    sizeof(optval))) != 0) {
				break;
			}

			error = inp_flush(sotoinpcb(so), optval);
			break;

		default:
			error = ip6_ctloutput(so, sopt);
			break;
		}
		break;
	}

	return error;
}

static int
rip6_attach(struct socket *so, int proto, struct proc *p)
{
	struct inpcb *__single inp;
	int error;

	inp = sotoinpcb(so);
	if (inp) {
		panic("rip6_attach");
	}
	if ((error = proc_suser(p)) != 0) {
		return error;
	}

	error = soreserve(so, rip_sendspace, rip_recvspace);
	if (error) {
		return error;
	}
	error = in_pcballoc(so, &ripcbinfo, p);
	if (error) {
		return error;
	}
	inp = (struct inpcb *)so->so_pcb;
	inp->inp_vflag |= INP_IPV6;
	inp->in6p_ip6_nxt = (char)proto;
	inp->in6p_hops = -1;    /* use kernel default */
	inp->in6p_cksum = -1;
	inp->in6p_icmp6filt = kalloc_type(struct icmp6_filter,
	    Z_WAITOK | Z_NOFAIL);
	ICMP6_FILTER_SETPASSALL(inp->in6p_icmp6filt);
	return 0;
}

static int
rip6_detach(struct socket *so)
{
	struct inpcb *__single inp;

	inp = sotoinpcb(so);
	if (inp == 0) {
		panic("rip6_detach");
	}
	/* xxx: RSVP */
	if (inp->in6p_icmp6filt) {
		kfree_type(struct icmp6_filter, inp->in6p_icmp6filt);
		inp->in6p_icmp6filt = NULL;
	}
	in6_pcbdetach(inp);
	return 0;
}

static int
rip6_abort(struct socket *so)
{
	soisdisconnected(so);
	return rip6_detach(so);
}

static int
rip6_disconnect(struct socket *so)
{
	struct inpcb *__single inp = sotoinpcb(so);

	if ((so->so_state & SS_ISCONNECTED) == 0) {
		return ENOTCONN;
	}
	inp->in6p_faddr = in6addr_any;
	inp->inp_fifscope = IFSCOPE_NONE;
	return rip6_abort(so);
}

static int
rip6_bind(struct socket *so, struct sockaddr *nam, struct proc *p)
{
#pragma unused(p)
	struct inpcb *__single inp = sotoinpcb(so);
	struct sockaddr_in6 sin6;
	struct ifaddr *__single ifa = NULL;
	struct ifnet *__single outif = NULL;
	uint32_t ifscope = IFSCOPE_NONE;
	int error;

	if (inp == NULL
#if NECP
	    || (necp_socket_should_use_flow_divert(inp))
#endif /* NECP */
	    ) {
		return inp == NULL ? EINVAL : EPROTOTYPE;
	}

	if (nam->sa_len != sizeof(struct sockaddr_in6)) {
		return EINVAL;
	}

	if (TAILQ_EMPTY(&ifnet_head) || SIN6(nam)->sin6_family != AF_INET6) {
		return EADDRNOTAVAIL;
	}

	bzero(&sin6, sizeof(sin6));
	*(&sin6) = *SIN6(nam);

	if ((error = sa6_embedscope(&sin6, ip6_use_defzone, &ifscope)) != 0) {
		return error;
	}

	/* Sanitize local copy for address searches */
	sin6.sin6_flowinfo = 0;
	sin6.sin6_port = 0;
	if (in6_embedded_scope) {
		sin6.sin6_scope_id = 0;
	}

	if (!IN6_IS_ADDR_UNSPECIFIED(&sin6.sin6_addr) &&
	    (ifa = ifa_ifwithaddr(SA(&sin6))) == 0) {
		return EADDRNOTAVAIL;
	}
	if (ifa != NULL) {
		IFA_LOCK(ifa);
		if ((ifatoia6(ifa))->ia6_flags &
		    (IN6_IFF_ANYCAST | IN6_IFF_NOTREADY | IN6_IFF_CLAT46 |
		    IN6_IFF_DETACHED | IN6_IFF_DEPRECATED)) {
			IFA_UNLOCK(ifa);
			ifa_remref(ifa);
			return EADDRNOTAVAIL;
		}
		outif = ifa->ifa_ifp;
		IFA_UNLOCK(ifa);
		ifa_remref(ifa);
	}
	inp->in6p_laddr = sin6.sin6_addr;
	inp->in6p_last_outifp = outif;
	inp->inp_lifscope = ifscope;
	in6_verify_ifscope(&inp->in6p_laddr, inp->inp_lifscope);
	return 0;
}

static int
rip6_connect(struct socket *so, struct sockaddr *nam, __unused struct proc *p)
{
	struct inpcb *__single inp = sotoinpcb(so);
	struct sockaddr_in6 *__single addr = (struct sockaddr_in6 *)(void *)nam;
	struct in6_addr *__single in6a = NULL;
	struct in6_addr storage;
	int error = 0;
#if ENABLE_DEFAULT_SCOPE
	struct sockaddr_in6 tmp;
#endif
	unsigned int ifscope;
	struct ifnet *__single outif = NULL;

	if (inp == NULL
#if NECP
	    || (necp_socket_should_use_flow_divert(inp))
#endif /* NECP */
	    ) {
		return inp == NULL ? EINVAL : EPROTOTYPE;
	}
	if (nam->sa_len != sizeof(*addr)) {
		return EINVAL;
	}
	if (TAILQ_EMPTY(&ifnet_head)) {
		return EADDRNOTAVAIL;
	}
	if (addr->sin6_family != AF_INET6) {
		return EAFNOSUPPORT;
	}

	if (!(so->so_flags1 & SOF1_CONNECT_COUNTED)) {
		so->so_flags1 |= SOF1_CONNECT_COUNTED;
		INC_ATOMIC_INT64_LIM(net_api_stats.nas_socket_inet6_dgram_connected);
	}

#if ENABLE_DEFAULT_SCOPE
	if (addr->sin6_scope_id == 0) { /* not change if specified  */
		/* avoid overwrites */
		tmp = *addr;
		addr = &tmp;
		addr->sin6_scope_id = scope6_addr2default(&addr->sin6_addr);
	}
#endif

	/* KAME hack: embed scopeid */
	if (in6_embedscope(&SIN6(nam)->sin6_addr, SIN6(nam), inp, NULL, NULL, IN6_NULL_IF_EMBEDDED_SCOPE(&SIN6(nam)->sin6_scope_id)) != 0) {
		return EINVAL;
	}

	ifscope = (inp->inp_flags & INP_BOUND_IF) ?
	    inp->inp_boundifp->if_index : IFSCOPE_NONE;

	/* Source address selection. XXX: need pcblookup? */
	struct ifnet *__single src_ifp = NULL;
	in6a = in6_selectsrc(addr, inp->in6p_outputopts, inp, &inp->in6p_route,
	    &src_ifp, &storage, ifscope, &error);
	if (src_ifp != NULL) {
		if (in6a != NULL) {
			inp->inp_lifscope = in6_addr2scopeid(src_ifp, in6a);
		}
		ifnet_release(src_ifp);
		src_ifp = NULL;
	}
	if (IN6_IS_SCOPE_EMBED(&addr->sin6_addr) && inp->inp_lifscope == IFSCOPE_NONE) {
		inp->inp_lifscope = addr->sin6_scope_id;
	}

	if (in6a == NULL) {
		return error ? error : EADDRNOTAVAIL;
	}
	inp->in6p_laddr = *in6a;
	inp->in6p_faddr = addr->sin6_addr;
	if (inp->in6p_route.ro_rt != NULL) {
		outif = inp->in6p_route.ro_rt->rt_ifp;
	}
	inp->in6p_last_outifp = outif;
	in6_verify_ifscope(&inp->in6p_laddr, inp->inp_lifscope);
	inp->inp_fifscope = addr->sin6_scope_id;
	in6_verify_ifscope(&inp->in6p_faddr, inp->inp_fifscope);

	soisconnected(so);
	return 0;
}

static int
rip6_shutdown(struct socket *so)
{
	socantsendmore(so);
	return 0;
}

static int
rip6_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct proc *p)
{
#pragma unused(flags, p)
	struct inpcb *__single inp = sotoinpcb(so);
	int error = 0;

	if (inp == NULL
#if NECP
	    || (necp_socket_should_use_flow_divert(inp))
#endif /* NECP */
	    ) {
		if (inp == NULL) {
			error = EINVAL;
		} else {
			error = EPROTOTYPE;
		}
		goto bad;
	}

	so_update_tx_data_stats(so, 1, m->m_pkthdr.len);

	return rip6_output(m, so, SIN6(nam), control, 1);

bad:
	VERIFY(error != 0);

	if (m != NULL) {
		m_freem(m);
	}
	if (control != NULL) {
		m_freem(control);
	}

	return error;
}

struct pr_usrreqs rip6_usrreqs = {
	.pru_abort =            rip6_abort,
	.pru_attach =           rip6_attach,
	.pru_bind =             rip6_bind,
	.pru_connect =          rip6_connect,
	.pru_control =          in6_control,
	.pru_detach =           rip6_detach,
	.pru_disconnect =       rip6_disconnect,
	.pru_peeraddr =         in6_getpeeraddr,
	.pru_send =             rip6_send,
	.pru_shutdown =         rip6_shutdown,
	.pru_sockaddr =         in6_getsockaddr,
	.pru_sosend =           sosend,
	.pru_soreceive =        soreceive,
};

__private_extern__ struct pr_usrreqs icmp6_dgram_usrreqs = {
	.pru_abort =            rip6_abort,
	.pru_attach =           icmp6_dgram_attach,
	.pru_bind =             rip6_bind,
	.pru_connect =          rip6_connect,
	.pru_control =          in6_control,
	.pru_detach =           rip6_detach,
	.pru_disconnect =       rip6_disconnect,
	.pru_peeraddr =         in6_getpeeraddr,
	.pru_send =             icmp6_dgram_send,
	.pru_shutdown =         rip6_shutdown,
	.pru_sockaddr =         in6_getsockaddr,
	.pru_sosend =           sosend,
	.pru_soreceive =        soreceive,
};
