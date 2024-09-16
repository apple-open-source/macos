/*
 * Copyright (c) 2000-2023 Apple Inc. All rights reserved.
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
 *	@(#)raw_ip.c	8.7 (Berkeley) 5/15/95
 */
/*
 * NOTICE: This file was modified by SPARTA, Inc. in 2005 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mcache.h>
#include <sys/proc.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <libkern/OSAtomic.h>
#include <kern/zalloc.h>

#include <pexpert/pexpert.h>

#include <net/if.h>
#include <net/net_api_stats.h>
#include <net/route.h>
#include <net/content_filter.h>
#include <net/sockaddr_utils.h>

#define _IP_VHL
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_tclass.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#include <netinet6/in6_pcb.h>


#if IPSEC
#include <netinet6/ipsec.h>
#endif /*IPSEC*/

#if DUMMYNET
#include <netinet/ip_dummynet.h>
#endif /* DUMMYNET */

int rip_detach(struct socket *);
int rip_abort(struct socket *);
int rip_disconnect(struct socket *);
int rip_bind(struct socket *, struct sockaddr *, struct proc *);
int rip_connect(struct socket *, struct sockaddr *, struct proc *);
int rip_shutdown(struct socket *);

struct  inpcbhead ripcb;
struct  inpcbinfo ripcbinfo;

/* control hooks for dummynet */
#if DUMMYNET
ip_dn_ctl_t *ip_dn_ctl_ptr;
#endif /* DUMMYNET */

/*
 * Nominal space allocated to a raw ip socket.
 */
#define RIPSNDQ         8192
#define RIPRCVQ         8192

static KALLOC_TYPE_DEFINE(ripzone, struct inpcb, NET_KT_DEFAULT);

/*
 * Raw interface to IP protocol.
 */

/*
 * Initialize raw connection block q.
 */
void
rip_init(struct protosw *pp, struct domain *dp)
{
#pragma unused(dp)
	static int rip_initialized = 0;
	struct inpcbinfo *pcbinfo;

	VERIFY((pp->pr_flags & (PR_INITIALIZED | PR_ATTACHED)) == PR_ATTACHED);

	if (rip_initialized) {
		return;
	}
	rip_initialized = 1;

	LIST_INIT(&ripcb);
	ripcbinfo.ipi_listhead = &ripcb;
	/*
	 * XXX We don't use the hash list for raw IP, but it's easier
	 * to allocate a one entry hash list than it is to check all
	 * over the place for ipi_hashbase == NULL.
	 */
	hashinit_counted_by(1, ripcbinfo.ipi_hashbase,
	    ripcbinfo.ipi_hashbase_count);
	ripcbinfo.ipi_hashmask = ripcbinfo.ipi_hashbase_count - 1;
	hashinit_counted_by(1, ripcbinfo.ipi_porthashbase,
	    ripcbinfo.ipi_porthashbase_count);
	ripcbinfo.ipi_porthashmask = ripcbinfo.ipi_porthashbase_count - 1;
	ripcbinfo.ipi_zone = ripzone;

	pcbinfo = &ripcbinfo;
	/*
	 * allocate lock group attribute and group for udp pcb mutexes
	 */
	pcbinfo->ipi_lock_grp = lck_grp_alloc_init("ripcb", LCK_GRP_ATTR_NULL);

	/*
	 * allocate the lock attribute for udp pcb mutexes
	 */
	lck_attr_setdefault(&pcbinfo->ipi_lock_attr);
	lck_rw_init(&pcbinfo->ipi_lock, pcbinfo->ipi_lock_grp,
	    &pcbinfo->ipi_lock_attr);

	in_pcbinfo_attach(&ripcbinfo);
}

static uint32_t
rip_inp_input(struct inpcb *inp, struct mbuf *m, int iphlen)
{
	struct ip *ip = mtod(m, struct ip *);
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct sockaddr_in ripsrc = {
		.sin_len = sizeof(ripsrc),
		.sin_family = AF_INET,
		.sin_port = 0,
		.sin_addr = { .s_addr = 0 },
		.sin_zero = {0, 0, 0, 0, 0, 0, 0, 0, }
	};
	mbuf_ref_t opts = NULL;
	boolean_t is_wake_pkt = false;
	uint32_t num_delivered = 0;

#if NECP
	if (!necp_socket_is_allowed_to_send_recv_v4(inp, 0, 0,
	    &ip->ip_dst, &ip->ip_src, ifp, 0, NULL, NULL, NULL, NULL)) {
		/* do not inject data to pcb */
		goto done;
	}
#endif /* NECP */

	ripsrc.sin_addr = ip->ip_src;

	if ((m->m_flags & M_PKTHDR) && (m->m_pkthdr.pkt_flags & PKTF_WAKE_PKT)) {
		is_wake_pkt = true;
	}

	if ((inp->inp_flags & INP_CONTROLOPTS) != 0 ||
	    SOFLOW_ENABLED(inp->inp_socket) ||
	    SO_RECV_CONTROL_OPTS(inp->inp_socket)) {
		if (ip_savecontrol(inp, &opts, ip, m) != 0) {
			m_freem(opts);
			goto done;
		}
	}
	if (inp->inp_flags & INP_STRIPHDR
#if CONTENT_FILTER
	    /*
	     * If socket is subject to Content Filter, delay stripping until reinject
	     */
	    && (!CFIL_DGRAM_FILTERED(inp->inp_socket))
#endif
	    ) {
		m->m_len -= iphlen;
		m->m_pkthdr.len -= iphlen;
		m->m_data += iphlen;
	}
	so_recv_data_stat(inp->inp_socket, m, 0);
	if (sbappendaddr(&inp->inp_socket->so_rcv,
	    (struct sockaddr *)&ripsrc, m, opts, NULL) != 0) {
		num_delivered = 1;
		sorwakeup(inp->inp_socket);
		if (is_wake_pkt) {
			soevent(inp->in6p_socket,
			    SO_FILT_HINT_LOCKED | SO_FILT_HINT_WAKE_PKT);
		}
	} else {
		ipstat.ips_raw_sappend_fail++;
	}
done:
	return num_delivered;
}

/*
 * The first pass is for IPv4 socket and the second pass for IPv6
 */
static bool
rip_input_inner(struct mbuf *m, int iphlen, bool is_ipv4_pass, uint32_t *total_delivered)
{
	struct inpcb *inp;
	struct inpcb *last = NULL;
	struct ip *ip = mtod(m, struct ip *);
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	bool need_ipv6_pass = false;
	uint32_t num_delivered = 0;

	lck_rw_lock_shared(&ripcbinfo.ipi_lock);
	LIST_FOREACH(inp, &ripcb, inp_list) {
		if (is_ipv4_pass) {
			if ((inp->inp_vflag & (INP_IPV4 | INP_IPV6)) != INP_IPV4) {
				/* Tell if we need to an IPv6 pass */
				need_ipv6_pass = true;
				continue;
			}
		} else {
			if ((inp->inp_vflag & (INP_IPV4 | INP_IPV6)) != (INP_IPV4 | INP_IPV6)) {
				continue;
			}
		}
		if (inp->inp_ip_p && (inp->inp_ip_p != ip->ip_p)) {
			continue;
		}
		if (inp->inp_laddr.s_addr &&
		    inp->inp_laddr.s_addr != ip->ip_dst.s_addr) {
			continue;
		}
		if (inp->inp_faddr.s_addr &&
		    inp->inp_faddr.s_addr != ip->ip_src.s_addr) {
			continue;
		}
		if (inp_restricted_recv(inp, ifp)) {
			continue;
		}
		if (last != NULL) {
			struct mbuf *n = m_copym_mode(m, 0, (int)M_COPYALL, M_DONTWAIT, NULL, NULL, M_COPYM_MUST_COPY_HDR);

			if (n == NULL) {
				continue;
			}
			num_delivered += rip_inp_input(last, n, iphlen);
		}
		last = inp;
	}

	/*
	 * Consume the orignal mbuf 'm' if:
	 * - it is the first pass and there is no IPv6 raw socket
	 * - it is the second pass for IPv6
	 */
	if (need_ipv6_pass == false || is_ipv4_pass == false) {
		if (last != NULL) {
			num_delivered += rip_inp_input(last, m, iphlen);
		} else {
			m_freem(m);
		}
	} else {
		if (last != NULL) {
			struct mbuf *n = m_copym_mode(m, 0, (int)M_COPYALL, M_DONTWAIT, NULL, NULL, M_COPYM_MUST_COPY_HDR);

			if (n != NULL) {
				num_delivered += rip_inp_input(last, n, iphlen);
			}
		}
	}
	/*
	 * Keep the list locked because socket filter may force the socket lock
	 * to be released when calling sbappendaddr() -- see rdar://7627704
	 */
	lck_rw_done(&ripcbinfo.ipi_lock);

	*total_delivered += num_delivered;

	return need_ipv6_pass;
}


/*
 * Setup generic address and protocol structures
 * for raw_input routine, then pass them along with
 * mbuf chain.
 */
void
rip_input(struct mbuf *m, int iphlen)
{
	uint32_t num_delivered = 0;
	bool need_v6_pass = false;

	/* Expect 32-bit aligned data pointer on strict-align platforms */
	MBUF_STRICT_DATA_ALIGNMENT_CHECK_32(m);

	/*
	 * First pass for raw IPv4 sockets that are protected by the inet_domain_mutex lock
	 */
	need_v6_pass = rip_input_inner(m, iphlen, true, &num_delivered);

	/*
	 * For the IPv6 pass we need to switch to the inet6_domain_mutex lock
	 * to protect the raw IPv6 sockets
	 */
	if (need_v6_pass) {
		lck_mtx_unlock(inet_domain_mutex);

		lck_mtx_lock(inet6_domain_mutex);
		rip_input_inner(m, iphlen, false, &num_delivered);
		lck_mtx_unlock(inet6_domain_mutex);

		lck_mtx_lock(inet_domain_mutex);
	}

	if (num_delivered > 0) {
		OSAddAtomic(1, &ipstat.ips_delivered);
	} else {
		OSAddAtomic(1, &ipstat.ips_noproto);
	}
}

/*
 * Generate IP header and pass packet to ip_output.
 * Tack on options user may have setup with control call.
 */
int
rip_output(
	struct mbuf *m,
	struct socket *so,
	u_int32_t dst,
	struct mbuf *control)
{
	struct ip *ip;
	struct inpcb *inp = sotoinpcb(so);
	int flags = (so->so_options & SO_DONTROUTE) | IP_ALLOWBROADCAST;
	int inp_flags = inp ? inp->inp_flags : 0;
	struct ip_out_args ipoa;
	struct ip_moptions *imo;
	int tos = IPTOS_UNSPEC;
	int error = 0;
#if CONTENT_FILTER
	struct m_tag *cfil_tag = NULL;
	bool cfil_faddr_use = false;
	uint32_t cfil_so_state_change_cnt = 0;
	uint32_t cfil_so_options = 0;
	int cfil_inp_flags = 0;
	struct sockaddr *__single cfil_faddr = NULL;
	struct sockaddr_in *__single cfil_sin;
	u_int32_t cfil_dst = 0;
#endif

#if CONTENT_FILTER
	/*
	 * If socket is subject to Content Filter and no addr is passed in,
	 * retrieve CFIL saved state from mbuf and use it if necessary.
	 */
	if (CFIL_DGRAM_FILTERED(so) && dst == INADDR_ANY) {
		cfil_tag = cfil_dgram_get_socket_state(m, &cfil_so_state_change_cnt, &cfil_so_options, &cfil_faddr, &cfil_inp_flags);
		if (cfil_tag) {
			cfil_sin = SIN(cfil_faddr);
			flags = (cfil_so_options & SO_DONTROUTE) | IP_ALLOWBROADCAST;
			inp_flags = cfil_inp_flags;
			if (inp && inp->inp_faddr.s_addr == INADDR_ANY) {
				/*
				 * Socket is unconnected, simply use the saved faddr as 'addr' to go through
				 * the connect/disconnect logic.
				 */
				dst = cfil_sin->sin_addr.s_addr;
			} else if ((so->so_state_change_cnt != cfil_so_state_change_cnt) &&
			    (inp->inp_fport != cfil_sin->sin_port ||
			    inp->inp_faddr.s_addr != cfil_sin->sin_addr.s_addr)) {
				/*
				 * Socket is connected but socket state and dest addr/port changed.
				 * We need to use the saved faddr and socket options.
				 */
				cfil_faddr_use = true;
				cfil_dst = cfil_sin->sin_addr.s_addr;
			}
			m_tag_free(cfil_tag);
		}
	}
#endif

	if (so->so_state & SS_ISCONNECTED) {
		if (dst != INADDR_ANY) {
			if (m != NULL) {
				m_freem(m);
			}
			if (control != NULL) {
				m_freem(control);
			}
			return EISCONN;
		}
		dst = cfil_faddr_use ? cfil_dst : inp->inp_faddr.s_addr;
	} else {
		if (dst == INADDR_ANY) {
			if (m != NULL) {
				m_freem(m);
			}
			if (control != NULL) {
				m_freem(control);
			}
			return ENOTCONN;
		}
	}

	bzero(&ipoa, sizeof(ipoa));
	ipoa.ipoa_boundif = IFSCOPE_NONE;
	ipoa.ipoa_flags = IPOAF_SELECT_SRCIF;

	int sotc = SO_TC_UNSPEC;
	int netsvctype = _NET_SERVICE_TYPE_UNSPEC;


	if (control != NULL) {
		tos = so_tos_from_control(control);
		sotc = so_tc_from_control(control, &netsvctype);

		m_freem(control);
		control = NULL;
	}
	if (sotc == SO_TC_UNSPEC) {
		sotc = so->so_traffic_class;
		netsvctype = so->so_netsvctype;
	}

	if (inp == NULL
#if NECP
	    || (necp_socket_should_use_flow_divert(inp))
#endif /* NECP */
	    ) {
		if (m != NULL) {
			m_freem(m);
		}
		VERIFY(control == NULL);
		return inp == NULL ? EINVAL : EPROTOTYPE;
	}

	flags |= IP_OUTARGS;
	/* If socket was bound to an ifindex, tell ip_output about it */
	if (inp->inp_flags & INP_BOUND_IF) {
		ipoa.ipoa_boundif = inp->inp_boundifp->if_index;
		ipoa.ipoa_flags |= IPOAF_BOUND_IF;
	}
	if (INP_NO_CELLULAR(inp)) {
		ipoa.ipoa_flags |=  IPOAF_NO_CELLULAR;
	}
	if (INP_NO_EXPENSIVE(inp)) {
		ipoa.ipoa_flags |=  IPOAF_NO_EXPENSIVE;
	}
	if (INP_NO_CONSTRAINED(inp)) {
		ipoa.ipoa_flags |=  IPOAF_NO_CONSTRAINED;
	}
	if (INP_AWDL_UNRESTRICTED(inp)) {
		ipoa.ipoa_flags |=  IPOAF_AWDL_UNRESTRICTED;
	}
	if (INP_MANAGEMENT_ALLOWED(inp)) {
		ipoa.ipoa_flags |=  IPOAF_MANAGEMENT_ALLOWED;
	}
	ipoa.ipoa_sotc = sotc;
	ipoa.ipoa_netsvctype = netsvctype;

	if (inp->inp_flowhash == 0) {
		inp_calc_flowhash(inp);
		ASSERT(inp->inp_flowhash != 0);
	}

	/*
	 * If the user handed us a complete IP packet, use it.
	 * Otherwise, allocate an mbuf for a header and fill it in.
	 */
	if ((inp_flags & INP_HDRINCL) == 0) {
		if (m->m_pkthdr.len + sizeof(struct ip) > IP_MAXPACKET) {
			m_freem(m);
			return EMSGSIZE;
		}
		M_PREPEND(m, sizeof(struct ip), M_WAIT, 1);
		if (m == NULL) {
			return ENOBUFS;
		}
		ip = mtod(m, struct ip *);
		if (tos != IPTOS_UNSPEC) {
			ip->ip_tos = (uint8_t)(tos & IPTOS_MASK);
		} else {
			ip->ip_tos = inp->inp_ip_tos;
		}
		if (inp->inp_flags2 & INP2_DONTFRAG) {
			ip->ip_off = IP_DF;
		} else {
			ip->ip_off = 0;
		}
		ip->ip_p = inp->inp_ip_p;
		ip->ip_len = (uint16_t)m->m_pkthdr.len;
		ip->ip_src = inp->inp_laddr;
		ip->ip_dst.s_addr = dst;
		ip->ip_ttl = inp->inp_ip_ttl;
	} else {
		if (m->m_pkthdr.len > IP_MAXPACKET) {
			m_freem(m);
			return EMSGSIZE;
		}
		ip = mtod(m, struct ip *);
		/*
		 * don't allow both user specified and setsockopt options,
		 * and don't allow packet length sizes that will crash
		 */
		if (m->m_pkthdr.len < sizeof(struct ip) ||
		    ((IP_VHL_HL(ip->ip_vhl) != (sizeof(*ip) >> 2)) && inp->inp_options) ||
		    (ip->ip_len > m->m_pkthdr.len) ||
		    (ip->ip_len < (IP_VHL_HL(ip->ip_vhl) << 2))) {
			m_freem(m);
			return EINVAL;
		}
		if (ip->ip_id == 0 && !(rfc6864 && IP_OFF_IS_ATOMIC(ntohs(ip->ip_off)))) {
			ip->ip_id = ip_randomid((uint64_t)m);
		}
		/* XXX prevent ip_output from overwriting header fields */
		flags |= IP_RAWOUTPUT;
		OSAddAtomic(1, &ipstat.ips_rawout);
	}

	if (inp->inp_laddr.s_addr != INADDR_ANY) {
		ipoa.ipoa_flags |= IPOAF_BOUND_SRCADDR;
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
		    ROUTE_UNUSABLE(&inp->inp_route))
#if CONTENT_FILTER
		    || cfil_faddr_use
#endif
		    ) {
			struct sockaddr_in to;
			struct sockaddr_in from;
			struct in_addr laddr = ip->ip_src;

			ROUTE_RELEASE(&inp->inp_route);

			bzero(&from, sizeof(struct sockaddr_in));
			from.sin_family = AF_INET;
			from.sin_len = sizeof(struct sockaddr_in);
			from.sin_addr = laddr;

			bzero(&to, sizeof(struct sockaddr_in));
			to.sin_family = AF_INET;
			to.sin_len = sizeof(struct sockaddr_in);
			to.sin_addr.s_addr = ip->ip_dst.s_addr;

			if ((error = in_pcbladdr(inp, (struct sockaddr *)&to,
			    &laddr, ipoa.ipoa_boundif, NULL, 1)) != 0) {
				printf("%s in_pcbladdr(%p) error %d\n",
				    __func__, inp, error);
				m_freem(m);
				return error;
			}

			inp_update_necp_policy(inp, (struct sockaddr *)&from,
			    (struct sockaddr *)&to, ipoa.ipoa_boundif);
			inp->inp_policyresult.results.qos_marking_gencount = 0;
		}

		if (!necp_socket_is_allowed_to_send_recv_v4(inp, 0, 0,
		    &ip->ip_src, &ip->ip_dst, NULL, 0, &policy_id, &route_rule_id, &skip_policy_id, &pass_flags)) {
			m_freem(m);
			return EHOSTUNREACH;
		}

		necp_mark_packet_from_socket(m, inp, policy_id, route_rule_id, skip_policy_id, pass_flags);

		if (net_qos_policy_restricted != 0) {
			struct ifnet *rt_ifp = NULL;

			if (inp->inp_route.ro_rt != NULL) {
				rt_ifp = inp->inp_route.ro_rt->rt_ifp;
			}

			necp_socket_update_qos_marking(inp, inp->inp_route.ro_rt, route_rule_id);
		}
	}
#endif /* NECP */
	if ((so->so_flags1 & SOF1_QOSMARKING_ALLOWED)) {
		ipoa.ipoa_flags |= IPOAF_QOSMARKING_ALLOWED;
	}
#if IPSEC
	if (inp->inp_sp != NULL && ipsec_setsocket(m, so) != 0) {
		m_freem(m);
		return ENOBUFS;
	}
#endif /*IPSEC*/

	if (ROUTE_UNUSABLE(&inp->inp_route)) {
		ROUTE_RELEASE(&inp->inp_route);
	}

	set_packet_service_class(m, so, sotc, 0);
	m->m_pkthdr.pkt_flowsrc = FLOWSRC_INPCB;
	m->m_pkthdr.pkt_flowid = inp->inp_flowhash;
	m->m_pkthdr.pkt_flags |= (PKTF_FLOW_ID | PKTF_FLOW_LOCALSRC |
	    PKTF_FLOW_RAWSOCK);
	m->m_pkthdr.pkt_proto = inp->inp_ip_p;
	m->m_pkthdr.tx_rawip_pid = so->last_pid;
	m->m_pkthdr.tx_rawip_e_pid = so->e_pid;
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

	imo = inp->inp_moptions;
	if (imo != NULL) {
		IMO_ADDREF(imo);
	}
	/*
	 * The domain lock is held across ip_output, so it is okay
	 * to pass the PCB cached route pointer directly to IP and
	 * the modules beneath it.
	 */
	// TODO: PASS DOWN ROUTE RULE ID
	error = ip_output(m, inp->inp_options, &inp->inp_route, flags,
	    imo, &ipoa);

	if (imo != NULL) {
		IMO_REMREF(imo);
	}

	if (inp->inp_route.ro_rt != NULL) {
		struct rtentry *rt = inp->inp_route.ro_rt;
		struct ifnet *outif;

		if ((rt->rt_flags & (RTF_MULTICAST | RTF_BROADCAST)) ||
		    inp->inp_socket == NULL ||
#if CONTENT_FILTER
		    /* Discard temporary route for cfil case */
		    cfil_faddr_use ||
#endif
		    !(inp->inp_socket->so_state & SS_ISCONNECTED)) {
			rt = NULL;      /* unusable */
		}
		/*
		 * Always discard the cached route for unconnected
		 * socket or if it is a multicast route.
		 */
		if (rt == NULL) {
			ROUTE_RELEASE(&inp->inp_route);
		}

		/*
		 * If this is a connected socket and the destination
		 * route is unicast, update outif with that of the
		 * route interface used by IP.
		 */
		if (rt != NULL &&
		    (outif = rt->rt_ifp) != inp->inp_last_outifp) {
			inp->inp_last_outifp = outif;
		}
	} else {
		ROUTE_RELEASE(&inp->inp_route);
	}

	/*
	 * If output interface was cellular/expensive/constrained, and this socket is
	 * denied access to it, generate an event.
	 */
	if (error != 0 && (ipoa.ipoa_flags & IPOAF_R_IFDENIED) &&
	    (INP_NO_CELLULAR(inp) || INP_NO_EXPENSIVE(inp) || INP_NO_CONSTRAINED(inp))) {
		soevent(so, (SO_FILT_HINT_LOCKED | SO_FILT_HINT_IFDENIED));
	}

	return error;
}


/*
 * Raw IP socket option processing.
 */
int
rip_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct  inpcb *inp = sotoinpcb(so);
	int     error, optval;

	/* Allow <SOL_SOCKET,SO_BINDTODEVICE> at this level */
	if (sopt->sopt_level == SOL_SOCKET && sopt->sopt_name == SO_BINDTODEVICE) {
		return ip_ctloutput(so, sopt);
	}

	if (sopt->sopt_level != IPPROTO_IP) {
		return EINVAL;
	}

	error = 0;

	switch (sopt->sopt_dir) {
	case SOPT_GET:
		switch (sopt->sopt_name) {
		case IP_HDRINCL:
			optval = inp->inp_flags & INP_HDRINCL;
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;

		case IP_STRIPHDR:
			optval = inp->inp_flags & INP_STRIPHDR;
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;


#if DUMMYNET
		case IP_DUMMYNET_GET:
			if (!DUMMYNET_LOADED) {
				ip_dn_init();
			}
			if (DUMMYNET_LOADED) {
				error = ip_dn_ctl_ptr(sopt);
			} else {
				error = ENOPROTOOPT;
			}
			break;
#endif /* DUMMYNET */

		default:
			error = ip_ctloutput(so, sopt);
			break;
		}
		break;

	case SOPT_SET:
		switch (sopt->sopt_name) {
		case IP_HDRINCL:
			error = sooptcopyin(sopt, &optval, sizeof optval,
			    sizeof optval);
			if (error) {
				break;
			}
			if (optval) {
				inp->inp_flags |= INP_HDRINCL;
			} else {
				inp->inp_flags &= ~INP_HDRINCL;
			}
			break;

		case IP_STRIPHDR:
			error = sooptcopyin(sopt, &optval, sizeof optval,
			    sizeof optval);
			if (error) {
				break;
			}
			if (optval) {
				inp->inp_flags |= INP_STRIPHDR;
			} else {
				inp->inp_flags &= ~INP_STRIPHDR;
			}
			break;


#if DUMMYNET
		case IP_DUMMYNET_CONFIGURE:
		case IP_DUMMYNET_DEL:
		case IP_DUMMYNET_FLUSH:
			if (!DUMMYNET_LOADED) {
				ip_dn_init();
			}
			if (DUMMYNET_LOADED) {
				error = ip_dn_ctl_ptr(sopt);
			} else {
				error = ENOPROTOOPT;
			}
			break;
#endif /* DUMMYNET */

		case SO_FLUSH:
			if ((error = sooptcopyin(sopt, &optval, sizeof(optval),
			    sizeof(optval))) != 0) {
				break;
			}

			error = inp_flush(inp, optval);
			break;

		default:
			error = ip_ctloutput(so, sopt);
			break;
		}
		break;
	}

	return error;
}

/*
 * This function exists solely to receive the PRC_IFDOWN messages which
 * are sent by if_down().  It looks for an ifaddr whose ifa_addr is sa,
 * and calls in_ifadown() to remove all routes corresponding to that address.
 * It also receives the PRC_IFUP messages from if_up() and reinstalls the
 * interface routes.
 */
void
rip_ctlinput(
	int cmd,
	struct sockaddr *sa,
	__unused void *vip,
	__unused struct ifnet *ifp)
{
	struct in_ifaddr *ia = NULL;
	struct ifnet *iaifp = NULL;
	int err = 0;
	int flags, done = 0;

	switch (cmd) {
	case PRC_IFDOWN:
		lck_rw_lock_shared(&in_ifaddr_rwlock);
		for (ia = in_ifaddrhead.tqh_first; ia;
		    ia = ia->ia_link.tqe_next) {
			IFA_LOCK(&ia->ia_ifa);
			if (ia->ia_ifa.ifa_addr == sa &&
			    (ia->ia_flags & IFA_ROUTE)) {
				done = 1;
				ifa_addref(&ia->ia_ifa);
				IFA_UNLOCK(&ia->ia_ifa);
				lck_rw_done(&in_ifaddr_rwlock);
				lck_mtx_lock(rnh_lock);
				/*
				 * in_ifscrub kills the interface route.
				 */
				in_ifscrub(ia->ia_ifp, ia, 1);
				/*
				 * in_ifadown gets rid of all the rest of
				 * the routes.  This is not quite the right
				 * thing to do, but at least if we are running
				 * a routing process they will come back.
				 */
				in_ifadown(&ia->ia_ifa, 1);
				lck_mtx_unlock(rnh_lock);
				ifa_remref(&ia->ia_ifa);
				break;
			}
			IFA_UNLOCK(&ia->ia_ifa);
		}
		if (!done) {
			lck_rw_done(&in_ifaddr_rwlock);
		}
		break;

	case PRC_IFUP:
		lck_rw_lock_shared(&in_ifaddr_rwlock);
		for (ia = in_ifaddrhead.tqh_first; ia;
		    ia = ia->ia_link.tqe_next) {
			IFA_LOCK(&ia->ia_ifa);
			if (ia->ia_ifa.ifa_addr == sa) {
				/* keep it locked */
				break;
			}
			IFA_UNLOCK(&ia->ia_ifa);
		}
		if (ia == NULL || (ia->ia_flags & IFA_ROUTE) ||
		    (ia->ia_ifa.ifa_debug & IFD_NOTREADY)) {
			if (ia != NULL) {
				IFA_UNLOCK(&ia->ia_ifa);
			}
			lck_rw_done(&in_ifaddr_rwlock);
			return;
		}
		ifa_addref(&ia->ia_ifa);
		IFA_UNLOCK(&ia->ia_ifa);
		lck_rw_done(&in_ifaddr_rwlock);

		flags = RTF_UP;
		iaifp = ia->ia_ifa.ifa_ifp;

		if ((iaifp->if_flags & IFF_LOOPBACK)
		    || (iaifp->if_flags & IFF_POINTOPOINT)) {
			flags |= RTF_HOST;
		}

		err = rtinit(&ia->ia_ifa, RTM_ADD, flags);
		if (err == 0) {
			IFA_LOCK_SPIN(&ia->ia_ifa);
			ia->ia_flags |= IFA_ROUTE;
			IFA_UNLOCK(&ia->ia_ifa);
		}
		ifa_remref(&ia->ia_ifa);
		break;
	}
}

u_int32_t       rip_sendspace = RIPSNDQ;
u_int32_t       rip_recvspace = RIPRCVQ;

SYSCTL_INT(_net_inet_raw, OID_AUTO, maxdgram, CTLFLAG_RW | CTLFLAG_LOCKED,
    &rip_sendspace, 0, "Maximum outgoing raw IP datagram size");
SYSCTL_INT(_net_inet_raw, OID_AUTO, recvspace, CTLFLAG_RW | CTLFLAG_LOCKED,
    &rip_recvspace, 0, "Maximum incoming raw IP datagram size");
SYSCTL_UINT(_net_inet_raw, OID_AUTO, pcbcount, CTLFLAG_RD | CTLFLAG_LOCKED,
    &ripcbinfo.ipi_count, 0, "Number of active PCBs");

static int
rip_attach(struct socket *so, int proto, struct proc *p)
{
	struct inpcb *inp;
	int error;

	inp = sotoinpcb(so);
	if (inp) {
		panic("rip_attach");
	}
	if ((so->so_state & SS_PRIV) == 0) {
		return EPERM;
	}
	if (proto > UINT8_MAX) {
		return EINVAL;
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
	inp->inp_vflag |= INP_IPV4;
	VERIFY(proto <= UINT8_MAX);
	inp->inp_ip_p = (u_char)proto;
	inp->inp_ip_ttl = (u_char)ip_defttl;
	return 0;
}

__private_extern__ int
rip_detach(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	if (inp == 0) {
		panic("rip_detach");
	}
	in_pcbdetach(inp);
	return 0;
}

__private_extern__ int
rip_abort(struct socket *so)
{
	soisdisconnected(so);
	return rip_detach(so);
}

__private_extern__ int
rip_disconnect(struct socket *so)
{
	if ((so->so_state & SS_ISCONNECTED) == 0) {
		return ENOTCONN;
	}
	return rip_abort(so);
}

__private_extern__ int
rip_bind(struct socket *so, struct sockaddr *nam, struct proc *p)
{
#pragma unused(p)
	struct inpcb *inp = sotoinpcb(so);
	struct sockaddr_in sin;
	struct ifaddr *ifa = NULL;
	struct ifnet *outif = NULL;

	if (inp == NULL
#if NECP
	    || (necp_socket_should_use_flow_divert(inp))
#endif /* NECP */
	    ) {
		return inp == NULL ? EINVAL : EPROTOTYPE;
	}

	if (nam->sa_len != sizeof(struct sockaddr_in)) {
		return EINVAL;
	}

	/* Sanitized local copy for interface address searches */
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(struct sockaddr_in);
	sin.sin_addr.s_addr = SIN(nam)->sin_addr.s_addr;

	if (TAILQ_EMPTY(&ifnet_head) ||
	    (sin.sin_family != AF_INET && sin.sin_family != AF_IMPLINK) ||
	    (sin.sin_addr.s_addr && (ifa = ifa_ifwithaddr(SA(&sin))) == 0)) {
		return EADDRNOTAVAIL;
	} else if (ifa) {
		/*
		 * Opportunistically determine the outbound
		 * interface that may be used; this may not
		 * hold true if we end up using a route
		 * going over a different interface, e.g.
		 * when sending to a local address.  This
		 * will get updated again after sending.
		 */
		IFA_LOCK(ifa);
		outif = ifa->ifa_ifp;
		IFA_UNLOCK(ifa);
		ifa_remref(ifa);
	}
	inp->inp_laddr = sin.sin_addr;
	inp->inp_last_outifp = outif;

	return 0;
}

__private_extern__ int
rip_connect(struct socket *so, struct sockaddr *nam, __unused  struct proc *p)
{
	struct inpcb *inp = sotoinpcb(so);
	struct sockaddr_in *addr = (struct sockaddr_in *)(void *)nam;

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
	if ((addr->sin_family != AF_INET) &&
	    (addr->sin_family != AF_IMPLINK)) {
		return EAFNOSUPPORT;
	}

	if (!(so->so_flags1 & SOF1_CONNECT_COUNTED)) {
		so->so_flags1 |= SOF1_CONNECT_COUNTED;
		INC_ATOMIC_INT64_LIM(net_api_stats.nas_socket_inet_dgram_connected);
	}

	inp->inp_faddr = addr->sin_addr;
	soisconnected(so);

	return 0;
}

__private_extern__ int
rip_shutdown(struct socket *so)
{
	socantsendmore(so);
	return 0;
}

__private_extern__ int
rip_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct proc *p)
{
#pragma unused(flags, p)
	struct inpcb *inp = sotoinpcb(so);
	u_int32_t dst = INADDR_ANY;
	int error = 0;

	if (inp == NULL
#if NECP
	    || (necp_socket_should_use_flow_divert(inp) && (error = EPROTOTYPE))
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

	if (nam != NULL) {
		dst = ((struct sockaddr_in *)(void *)nam)->sin_addr.s_addr;
	}
	return rip_output(m, so, dst, control);

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

/* note: rip_unlock is called from different protos  instead of the generic socket_unlock,
 * it will handle the socket dealloc on last reference
 * */
int
rip_unlock(struct socket *so, int refcount, void *debug)
{
	void *__single lr_saved;
	struct inpcb *inp = sotoinpcb(so);

	if (debug == NULL) {
		lr_saved = __unsafe_forge_single(void *, __builtin_return_address(0));
	} else {
		lr_saved = debug;
	}

	if (refcount) {
		if (so->so_usecount <= 0) {
			panic("rip_unlock: bad refoucnt so=%p val=%x lrh= %s",
			    so, so->so_usecount, solockhistory_nr(so));
			/* NOTREACHED */
		}
		so->so_usecount--;
		if (so->so_usecount == 0 && (inp->inp_wantcnt == WNT_STOPUSING)) {
			/* cleanup after last reference */
			lck_mtx_unlock(so->so_proto->pr_domain->dom_mtx);
			lck_rw_lock_exclusive(&ripcbinfo.ipi_lock);
			if (inp->inp_state != INPCB_STATE_DEAD) {
				if (SOCK_CHECK_DOM(so, PF_INET6)) {
					in6_pcbdetach(inp);
				} else {
					in_pcbdetach(inp);
				}
			}
			in_pcbdispose(inp);
			lck_rw_done(&ripcbinfo.ipi_lock);
			return 0;
		}
	}
	so->unlock_lr[so->next_unlock_lr] = lr_saved;
	so->next_unlock_lr = (so->next_unlock_lr + 1) % SO_LCKDBG_MAX;
	lck_mtx_unlock(so->so_proto->pr_domain->dom_mtx);
	return 0;
}

static int
rip_pcblist SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int error, i, n, sz;
	struct inpcb *inp, **inp_list;
	inp_gen_t gencnt;
	struct xinpgen xig;

	/*
	 * The process of preparing the TCB list is too time-consuming and
	 * resource-intensive to repeat twice on every request.
	 */
	lck_rw_lock_exclusive(&ripcbinfo.ipi_lock);
	if (req->oldptr == USER_ADDR_NULL) {
		n = ripcbinfo.ipi_count;
		req->oldidx = 2 * (sizeof xig)
		    + (n + n / 8) * sizeof(struct xinpcb);
		lck_rw_done(&ripcbinfo.ipi_lock);
		return 0;
	}

	if (req->newptr != USER_ADDR_NULL) {
		lck_rw_done(&ripcbinfo.ipi_lock);
		return EPERM;
	}

	/*
	 * OK, now we're committed to doing something.
	 */
	gencnt = ripcbinfo.ipi_gencnt;
	sz = n = ripcbinfo.ipi_count;

	bzero(&xig, sizeof(xig));
	xig.xig_len = sizeof xig;
	xig.xig_count = n;
	xig.xig_gen = gencnt;
	xig.xig_sogen = so_gencnt;
	error = SYSCTL_OUT(req, &xig, sizeof xig);
	if (error) {
		lck_rw_done(&ripcbinfo.ipi_lock);
		return error;
	}
	/*
	 * We are done if there is no pcb
	 */
	if (n == 0) {
		lck_rw_done(&ripcbinfo.ipi_lock);
		return 0;
	}

	inp_list = kalloc_type(struct inpcb *, n, Z_WAITOK);
	if (inp_list == NULL) {
		lck_rw_done(&ripcbinfo.ipi_lock);
		return ENOMEM;
	}

	for (inp = ripcbinfo.ipi_listhead->lh_first, i = 0; inp && i < n;
	    inp = inp->inp_list.le_next) {
		if (inp->inp_gencnt <= gencnt && inp->inp_state != INPCB_STATE_DEAD) {
			inp_list[i++] = inp;
		}
	}
	n = i;

	error = 0;
	for (i = 0; i < n; i++) {
		inp = inp_list[i];
		if (inp->inp_gencnt <= gencnt && inp->inp_state != INPCB_STATE_DEAD) {
			struct xinpcb xi;

			bzero(&xi, sizeof(xi));
			xi.xi_len = sizeof xi;
			/* XXX should avoid extra copy */
			inpcb_to_compat(inp, &xi.xi_inp);
			if (inp->inp_socket) {
				sotoxsocket(inp->inp_socket, &xi.xi_socket);
			}
			error = SYSCTL_OUT(req, &xi, sizeof xi);
		}
	}
	if (!error) {
		/*
		 * Give the user an updated idea of our state.
		 * If the generation differs from what we told
		 * her before, she knows that something happened
		 * while we were processing this request, and it
		 * might be necessary to retry.
		 */
		bzero(&xig, sizeof(xig));
		xig.xig_len = sizeof xig;
		xig.xig_gen = ripcbinfo.ipi_gencnt;
		xig.xig_sogen = so_gencnt;
		xig.xig_count = ripcbinfo.ipi_count;
		error = SYSCTL_OUT(req, &xig, sizeof xig);
	}

	lck_rw_done(&ripcbinfo.ipi_lock);
	kfree_type(struct inpcb *, sz, inp_list);
	return error;
}

SYSCTL_PROC(_net_inet_raw, OID_AUTO /*XXX*/, pcblist,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED, 0, 0,
    rip_pcblist, "S,xinpcb", "List of active raw IP sockets");

#if XNU_TARGET_OS_OSX

static int
rip_pcblist64 SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int error, i, n, sz;
	struct inpcb *inp, **inp_list;
	inp_gen_t gencnt;
	struct xinpgen xig;

	/*
	 * The process of preparing the TCB list is too time-consuming and
	 * resource-intensive to repeat twice on every request.
	 */
	lck_rw_lock_exclusive(&ripcbinfo.ipi_lock);
	if (req->oldptr == USER_ADDR_NULL) {
		n = ripcbinfo.ipi_count;
		req->oldidx = 2 * (sizeof xig)
		    + (n + n / 8) * sizeof(struct xinpcb64);
		lck_rw_done(&ripcbinfo.ipi_lock);
		return 0;
	}

	if (req->newptr != USER_ADDR_NULL) {
		lck_rw_done(&ripcbinfo.ipi_lock);
		return EPERM;
	}

	/*
	 * OK, now we're committed to doing something.
	 */
	gencnt = ripcbinfo.ipi_gencnt;
	sz = n = ripcbinfo.ipi_count;

	bzero(&xig, sizeof(xig));
	xig.xig_len = sizeof xig;
	xig.xig_count = n;
	xig.xig_gen = gencnt;
	xig.xig_sogen = so_gencnt;
	error = SYSCTL_OUT(req, &xig, sizeof xig);
	if (error) {
		lck_rw_done(&ripcbinfo.ipi_lock);
		return error;
	}
	/*
	 * We are done if there is no pcb
	 */
	if (n == 0) {
		lck_rw_done(&ripcbinfo.ipi_lock);
		return 0;
	}

	inp_list = kalloc_type(struct inpcb *, n, Z_WAITOK);
	if (inp_list == NULL) {
		lck_rw_done(&ripcbinfo.ipi_lock);
		return ENOMEM;
	}

	for (inp = ripcbinfo.ipi_listhead->lh_first, i = 0; inp && i < n;
	    inp = inp->inp_list.le_next) {
		if (inp->inp_gencnt <= gencnt && inp->inp_state != INPCB_STATE_DEAD) {
			inp_list[i++] = inp;
		}
	}
	n = i;

	error = 0;
	for (i = 0; i < n; i++) {
		inp = inp_list[i];
		if (inp->inp_gencnt <= gencnt && inp->inp_state != INPCB_STATE_DEAD) {
			struct xinpcb64 xi;

			bzero(&xi, sizeof(xi));
			xi.xi_len = sizeof xi;
			inpcb_to_xinpcb64(inp, &xi);
			if (inp->inp_socket) {
				sotoxsocket64(inp->inp_socket, &xi.xi_socket);
			}
			error = SYSCTL_OUT(req, &xi, sizeof xi);
		}
	}
	if (!error) {
		/*
		 * Give the user an updated idea of our state.
		 * If the generation differs from what we told
		 * her before, she knows that something happened
		 * while we were processing this request, and it
		 * might be necessary to retry.
		 */
		bzero(&xig, sizeof(xig));
		xig.xig_len = sizeof xig;
		xig.xig_gen = ripcbinfo.ipi_gencnt;
		xig.xig_sogen = so_gencnt;
		xig.xig_count = ripcbinfo.ipi_count;
		error = SYSCTL_OUT(req, &xig, sizeof xig);
	}

	lck_rw_done(&ripcbinfo.ipi_lock);
	kfree_type(struct inpcb *, sz, inp_list);
	return error;
}

SYSCTL_PROC(_net_inet_raw, OID_AUTO, pcblist64,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED, 0, 0,
    rip_pcblist64, "S,xinpcb64", "List of active raw IP sockets");

#endif /* XNU_TARGET_OS_OSX */


static int
rip_pcblist_n SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int error = 0;

	error = get_pcblist_n(IPPROTO_IP, req, &ripcbinfo);

	return error;
}

SYSCTL_PROC(_net_inet_raw, OID_AUTO, pcblist_n,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED, 0, 0,
    rip_pcblist_n, "S,xinpcb_n", "List of active raw IP sockets");

struct pr_usrreqs rip_usrreqs = {
	.pru_abort =            rip_abort,
	.pru_attach =           rip_attach,
	.pru_bind =             rip_bind,
	.pru_connect =          rip_connect,
	.pru_control =          in_control,
	.pru_detach =           rip_detach,
	.pru_disconnect =       rip_disconnect,
	.pru_peeraddr =         in_getpeeraddr,
	.pru_send =             rip_send,
	.pru_shutdown =         rip_shutdown,
	.pru_sockaddr =         in_getsockaddr,
	.pru_sosend =           sosend,
	.pru_soreceive =        soreceive,
};
/* DSEP Review Done pl-20051213-v02 @3253 */
