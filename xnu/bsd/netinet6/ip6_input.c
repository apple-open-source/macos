/*
 * Copyright (c) 2003-2024 Apple Inc. All rights reserved.
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
 *	@(#)ip_input.c	8.2 (Berkeley) 1/4/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/kauth.h>
#include <sys/mcache.h>

#include <mach/mach_time.h>
#include <mach/sdt.h>
#include <pexpert/pexpert.h>
#include <dev/random/randomdev.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/kpi_protocol.h>
#include <net/ntstat.h>
#include <net/init.h>
#include <net/net_osdep.h>
#include <net/net_perf.h>
#include <net/if_ports_used.h>
#include <net/droptap.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#if INET
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#endif /* INET */
#include <netinet/kpi_ipfilter_var.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#include <netinet/in_pcb.h>
#include <netinet/icmp6.h>
#include <netinet6/in6_ifattach.h>
#include <netinet6/nd6.h>
#include <netinet6/scope6_var.h>
#include <netinet6/ip6protosw.h>

#if IPSEC
#include <netinet6/ipsec.h>
#include <netinet6/ipsec6.h>
extern int ipsec_bypass;
#endif /* IPSEC */

#if DUMMYNET
#include <netinet/ip_dummynet.h>
#endif /* DUMMYNET */

/* we need it for NLOOP. */
#include "loop.h"

#if PF
#include <net/pfvar.h>
#endif /* PF */

#include <os/log.h>

struct ip6protosw *ip6_protox[IPPROTO_MAX];

static LCK_GRP_DECLARE(in6_ifaddr_rwlock_grp, "in6_ifaddr_rwlock");
LCK_RW_DECLARE(in6_ifaddr_rwlock, &in6_ifaddr_rwlock_grp);

/* Protected by in6_ifaddr_rwlock */
struct in6_ifaddrhead in6_ifaddrhead;
uint32_t in6_ifaddrlist_genid = 0;

#define IN6ADDR_NHASH    61
u_int32_t in6addr_hashp = 0;                  /* next largest prime */
u_int32_t in6addr_nhash = 0;                  /* hash table size */
struct in6_ifaddrhashhead *__counted_by(in6addr_nhash) in6_ifaddrhashtbl = 0;

#define IN6_IFSTAT_REQUIRE_ALIGNED_64(f)        \
	_CASSERT(!(offsetof(struct in6_ifstat, f) % sizeof (uint64_t)))

#define ICMP6_IFSTAT_REQUIRE_ALIGNED_64(f)      \
	_CASSERT(!(offsetof(struct icmp6_ifstat, f) % sizeof (uint64_t)))

struct ip6stat ip6stat;

LCK_ATTR_DECLARE(ip6_mutex_attr, 0, 0);
LCK_GRP_DECLARE(ip6_mutex_grp, "ip6");

LCK_MTX_DECLARE_ATTR(proxy6_lock, &ip6_mutex_grp, &ip6_mutex_attr);
LCK_MTX_DECLARE_ATTR(nd6_mutex_data, &ip6_mutex_grp, &ip6_mutex_attr);

extern int loopattach_done;
extern void addrsel_policy_init(void);

static int sysctl_reset_ip6_input_stats SYSCTL_HANDLER_ARGS;
static int sysctl_ip6_input_measure_bins SYSCTL_HANDLER_ARGS;
static int sysctl_ip6_input_getperf SYSCTL_HANDLER_ARGS;
static void ip6_init_delayed(void);
static int ip6_hopopts_input(u_int32_t *, u_int32_t *, struct mbuf **, int *);

static void in6_ifaddrhashtbl_init(void);

static struct m_tag *m_tag_kalloc_inet6(u_int32_t id, u_int16_t type, uint16_t len, int wait);
static void m_tag_kfree_inet6(struct m_tag *tag);

#if NSTF
extern void stfattach(void);
#endif /* NSTF */

SYSCTL_DECL(_net_inet6_ip6);

static uint32_t ip6_adj_clear_hwcksum = 0;
SYSCTL_UINT(_net_inet6_ip6, OID_AUTO, adj_clear_hwcksum,
    CTLFLAG_RW | CTLFLAG_LOCKED, &ip6_adj_clear_hwcksum, 0,
    "Invalidate hwcksum info when adjusting length");

static uint32_t ip6_adj_partial_sum = 1;
SYSCTL_UINT(_net_inet6_ip6, OID_AUTO, adj_partial_sum,
    CTLFLAG_RW | CTLFLAG_LOCKED, &ip6_adj_partial_sum, 0,
    "Perform partial sum adjustment of trailing bytes at IP layer");

static int ip6_input_measure = 0;
SYSCTL_PROC(_net_inet6_ip6, OID_AUTO, input_perf,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    &ip6_input_measure, 0, sysctl_reset_ip6_input_stats, "I", "Do time measurement");

static uint64_t ip6_input_measure_bins = 0;
SYSCTL_PROC(_net_inet6_ip6, OID_AUTO, input_perf_bins,
    CTLTYPE_QUAD | CTLFLAG_RW | CTLFLAG_LOCKED, &ip6_input_measure_bins, 0,
    sysctl_ip6_input_measure_bins, "I",
    "bins for chaining performance data histogram");

static net_perf_t net_perf;
SYSCTL_PROC(_net_inet6_ip6, OID_AUTO, input_perf_data,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, sysctl_ip6_input_getperf, "S,net_perf",
    "IP6 input performance data (struct net_perf, net/net_perf.h)");

/*
 * ip6_checkinterface controls the receive side of the models for multihoming
 * that are discussed in RFC 1122.
 *
 * sysctl_ip6_checkinterface values are:
 *  IP6_CHECKINTERFACE_WEAK_ES:
 *	This corresponds to the Weak End-System model where incoming packets from
 *	any interface are accepted provided the destination address of the incoming packet
 *	is assigned to some interface.
 *
 *  IP6_CHECKINTERFACE_HYBRID_ES:
 *	The Hybrid End-System model use the Strong End-System for tunnel interfaces
 *	(ipsec and utun) and the weak End-System model for other interfaces families.
 *	This prevents a rogue middle box to probe for signs of TCP connections
 *	that use the tunnel interface.
 *
 *  IP6_CHECKINTERFACE_STRONG_ES:
 *	The Strong model model requires the packet arrived on an interface that
 *	is assigned the destination address of the packet.
 *
 * Since the routing table and transmit implementation do not implement the Strong ES model,
 * setting this to a value different from IP6_CHECKINTERFACE_WEAK_ES may lead to unexpected results.
 *
 * When forwarding is enabled, the system reverts to the Weak ES model as a router
 * is expected by design to receive packets from several interfaces to the same address.
 */
#define IP6_CHECKINTERFACE_WEAK_ES       0
#define IP6_CHECKINTERFACE_HYBRID_ES     1
#define IP6_CHECKINTERFACE_STRONG_ES     2

static int ip6_checkinterface = IP6_CHECKINTERFACE_HYBRID_ES;

static int sysctl_ip6_checkinterface SYSCTL_HANDLER_ARGS;
SYSCTL_PROC(_net_inet6_ip6, OID_AUTO, check_interface,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_ip6_checkinterface, "I", "Verify packet arrives on correct interface");

#if (DEBUG || DEVELOPMENT)
#define IP6_CHECK_IFDEBUG 1
#else
#define IP6_CHECK_IFDEBUG 0
#endif /* (DEBUG || DEVELOPMENT) */
static int ip6_checkinterface_debug = IP6_CHECK_IFDEBUG;
SYSCTL_INT(_net_inet6_ip6, OID_AUTO, checkinterface_debug, CTLFLAG_RW | CTLFLAG_LOCKED,
    &ip6_checkinterface_debug, IP6_CHECK_IFDEBUG, "");

typedef enum ip6_check_if_result {
	IP6_CHECK_IF_NONE = 0,
	IP6_CHECK_IF_OURS = 1,
	IP6_CHECK_IF_DROP = 2,
	IP6_CHECK_IF_FORWARD = 3
} ip6_check_if_result_t;

static ip6_check_if_result_t ip6_input_check_interface(struct mbuf *, struct ip6_hdr *, struct ifnet *, struct route_in6 *rin6, struct ifnet **);

/*
 * On platforms which require strict alignment (currently for anything but
 * i386 or x86_64 or arm64), check if the IP header pointer is 32-bit aligned; if not,
 * copy the contents of the mbuf chain into a new chain, and free the original
 * one.  Create some head room in the first mbuf of the new chain, in case
 * it's needed later on.
 *
 * RFC 2460 says that IPv6 headers are 64-bit aligned, but network interfaces
 * mostly align to 32-bit boundaries.  Care should be taken never to use 64-bit
 * load/store operations on the fields in IPv6 headers.
 */
#if defined(__i386__) || defined(__x86_64__) || defined(__arm64__)
#define IP6_HDR_ALIGNMENT_FIXUP(_m, _ifp, _action) do { } while (0)
#else /* !__i386__ && !__x86_64__ && !__arm64__ */
#define IP6_HDR_ALIGNMENT_FIXUP(_m, _ifp, _action) do {             \
	if (!IP6_HDR_ALIGNED_P(mtod(_m, caddr_t))) {                    \
	        mbuf_ref_t _n;                                          \
	        ifnet_ref_t __ifp = (_ifp);                             \
	        os_atomic_inc(&(__ifp)->if_alignerrs, relaxed);         \
	        if (((_m)->m_flags & M_PKTHDR) &&                       \
	            (_m)->m_pkthdr.pkt_hdr != NULL)                     \
	                (_m)->m_pkthdr.pkt_hdr = NULL;                  \
	        _n = m_defrag_offset(_m, max_linkhdr, M_NOWAIT);        \
	        if (_n == NULL) {                                       \
	                ip6stat.ip6s_toosmall++;                        \
	                m_drop(_m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_TOO_SMALL, NULL, 0); \
	                (_m) = NULL;                                    \
	                _action;                                        \
	        } else {                                                \
	                VERIFY(_n != (_m));                             \
	                (_m) = _n;                                      \
	        }                                                       \
	}                                                               \
} while (0)
#endif /* !__i386__ && !__x86_64___ && !__arm64__ */

static void
ip6_proto_input(protocol_family_t protocol, mbuf_t packet)
{
#pragma unused(protocol)
#if INET
	struct timeval start_tv;
	if (ip6_input_measure) {
		net_perf_start_time(&net_perf, &start_tv);
	}
#endif /* INET */
	ip6_input(packet);
#if INET
	if (ip6_input_measure) {
		net_perf_measure_time(&net_perf, &start_tv, 1);
		net_perf_histogram(&net_perf, 1);
	}
#endif /* INET */
}

/*
 * IP6 initialization: fill in IP6 protocol switch table.
 * All protocols not implemented in kernel go to raw IP6 protocol handler.
 */
void
ip6_init(struct ip6protosw *pp, struct domain *dp)
{
	static int ip6_initialized = 0;
	struct protosw *__single pr;
	struct timeval tv;
	int i;
	domain_unguard_t __single unguard;

	domain_proto_mtx_lock_assert_held();
	VERIFY((pp->pr_flags & (PR_INITIALIZED | PR_ATTACHED)) == PR_ATTACHED);

	_CASSERT((sizeof(struct ip6_hdr) +
	    sizeof(struct icmp6_hdr)) <= _MHLEN);

	if (ip6_initialized) {
		return;
	}
	ip6_initialized = 1;

	eventhandler_lists_ctxt_init(&in6_evhdlr_ctxt);
	(void)EVENTHANDLER_REGISTER(&in6_evhdlr_ctxt, in6_event,
	    &in6_eventhdlr_callback, eventhandler_entry_dummy_arg,
	    EVENTHANDLER_PRI_ANY);

	eventhandler_lists_ctxt_init(&in6_clat46_evhdlr_ctxt);
	(void)EVENTHANDLER_REGISTER(&in6_clat46_evhdlr_ctxt, in6_clat46_event,
	    &in6_clat46_eventhdlr_callback, eventhandler_entry_dummy_arg,
	    EVENTHANDLER_PRI_ANY);

	for (i = 0; i < IN6_EVENT_MAX; i++) {
		VERIFY(in6_event2kev_array[i].in6_event_code == i);
	}

	pr = pffindproto_locked(PF_INET6, IPPROTO_RAW, SOCK_RAW);
	if (pr == NULL) {
		panic("%s: Unable to find [PF_INET6,IPPROTO_RAW,SOCK_RAW]",
		    __func__);
		/* NOTREACHED */
	}

	/* Initialize the entire ip6_protox[] array to IPPROTO_RAW. */
	for (i = 0; i < IPPROTO_MAX; i++) {
		ip6_protox[i] = (struct ip6protosw *)pr;
	}
	/*
	 * Cycle through IP protocols and put them into the appropriate place
	 * in ip6_protox[], skipping protocols IPPROTO_{IP,RAW}.
	 */
	VERIFY(dp == inet6domain && dp->dom_family == PF_INET6);
	TAILQ_FOREACH(pr, &dp->dom_protosw, pr_entry) {
		VERIFY(pr->pr_domain == dp);
		if (pr->pr_protocol != 0 && pr->pr_protocol != IPPROTO_RAW) {
			/* Be careful to only index valid IP protocols. */
			if (pr->pr_protocol < IPPROTO_MAX) {
				ip6_protox[pr->pr_protocol] =
				    (struct ip6protosw *)pr;
			}
		}
	}

	TAILQ_INIT(&in6_ifaddrhead);
	in6_ifaddrhashtbl_init();

	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_receive);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_hdrerr);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_toobig);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_noroute);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_addrerr);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_protounknown);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_truncated);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_discard);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_deliver);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_forward);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_request);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_discard);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_fragok);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_fragfail);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_fragcreat);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_reass_reqd);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_reass_ok);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_reass_fail);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_mcast);
	IN6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_mcast);

	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_msg);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_error);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_dstunreach);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_adminprohib);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_timeexceed);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_paramprob);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_pkttoobig);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_echo);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_echoreply);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_routersolicit);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_routeradvert);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_neighborsolicit);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_neighboradvert);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_redirect);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_mldquery);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_mldreport);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_in_mlddone);

	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_msg);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_error);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_dstunreach);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_adminprohib);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_timeexceed);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_paramprob);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_pkttoobig);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_echo);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_echoreply);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_routersolicit);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_routeradvert);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_neighborsolicit);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_neighboradvert);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_redirect);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_mldquery);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_mldreport);
	ICMP6_IFSTAT_REQUIRE_ALIGNED_64(ifs6_out_mlddone);

	getmicrotime(&tv);
	ip6_desync_factor =
	    (RandomULong() ^ tv.tv_usec) % MAX_TEMP_DESYNC_FACTOR;

	PE_parse_boot_argn("in6_embedded_scope", &in6_embedded_scope, sizeof(in6_embedded_scope));
	PE_parse_boot_argn("ip6_checkinterface", &i, sizeof(i));
	switch (i) {
	case IP6_CHECKINTERFACE_WEAK_ES:
	case IP6_CHECKINTERFACE_HYBRID_ES:
	case IP6_CHECKINTERFACE_STRONG_ES:
		ip6_checkinterface = i;
		break;
	default:
		break;
	}

	in6_ifaddr_init();
	ip6_moptions_init();
	nd6_init();
	frag6_init();
	icmp6_init(NULL, dp);
	addrsel_policy_init();

	/*
	 * P2P interfaces often route the local address to the loopback
	 * interface. At this point, lo0 hasn't been initialized yet, which
	 * means that we need to delay the IPv6 configuration of lo0.
	 */
	net_init_add(ip6_init_delayed);

	unguard = domain_unguard_deploy();
	i = proto_register_input(PF_INET6, ip6_proto_input, NULL, 0);
	if (i != 0) {
		panic("%s: failed to register PF_INET6 protocol: %d",
		    __func__, i);
		/* NOTREACHED */
	}
	domain_unguard_release(unguard);
}

static void
ip6_init_delayed(void)
{
	(void) in6_ifattach_prelim(lo_ifp);

	/* timer for regeneranation of temporary addresses randomize ID */
	timeout(in6_tmpaddrtimer, NULL,
	    (ip6_temp_preferred_lifetime - ip6_desync_factor -
	    ip6_temp_regen_advance) * hz);

#if NSTF
	stfattach();
#endif /* NSTF */
}

static void
ip6_input_adjust(struct mbuf *m, struct ip6_hdr *ip6, uint32_t plen,
    struct ifnet *inifp)
{
	boolean_t adjust = TRUE;
	uint32_t tot_len = sizeof(*ip6) + plen;

	ASSERT(m_pktlen(m) > tot_len);

	/*
	 * Invalidate hardware checksum info if ip6_adj_clear_hwcksum
	 * is set; useful to handle buggy drivers.  Note that this
	 * should not be enabled by default, as we may get here due
	 * to link-layer padding.
	 */
	if (ip6_adj_clear_hwcksum &&
	    (m->m_pkthdr.csum_flags & CSUM_DATA_VALID) &&
	    !(inifp->if_flags & IFF_LOOPBACK) &&
	    !(m->m_pkthdr.pkt_flags & PKTF_LOOP)) {
		m->m_pkthdr.csum_flags &= ~CSUM_DATA_VALID;
		m->m_pkthdr.csum_data = 0;
		ip6stat.ip6s_adj_hwcsum_clr++;
	}

	/*
	 * If partial checksum information is available, subtract
	 * out the partial sum of postpended extraneous bytes, and
	 * update the checksum metadata accordingly.  By doing it
	 * here, the upper layer transport only needs to adjust any
	 * prepended extraneous bytes (else it will do both.)
	 */
	if (ip6_adj_partial_sum &&
	    (m->m_pkthdr.csum_flags & (CSUM_DATA_VALID | CSUM_PARTIAL)) ==
	    (CSUM_DATA_VALID | CSUM_PARTIAL)) {
		m->m_pkthdr.csum_rx_val = m_adj_sum16(m,
		    m->m_pkthdr.csum_rx_start, m->m_pkthdr.csum_rx_start,
		    (tot_len - m->m_pkthdr.csum_rx_start),
		    m->m_pkthdr.csum_rx_val);
	} else if ((m->m_pkthdr.csum_flags &
	    (CSUM_DATA_VALID | CSUM_PARTIAL)) ==
	    (CSUM_DATA_VALID | CSUM_PARTIAL)) {
		/*
		 * If packet has partial checksum info and we decided not
		 * to subtract the partial sum of postpended extraneous
		 * bytes here (not the default case), leave that work to
		 * be handled by the other layers.  For now, only TCP, UDP
		 * layers are capable of dealing with this.  For all other
		 * protocols (including fragments), trim and ditch the
		 * partial sum as those layers might not implement partial
		 * checksumming (or adjustment) at all.
		 */
		if (ip6->ip6_nxt == IPPROTO_TCP ||
		    ip6->ip6_nxt == IPPROTO_UDP) {
			adjust = FALSE;
		} else {
			m->m_pkthdr.csum_flags &= ~CSUM_DATA_VALID;
			m->m_pkthdr.csum_data = 0;
			ip6stat.ip6s_adj_hwcsum_clr++;
		}
	}

	if (adjust) {
		ip6stat.ip6s_adj++;
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = tot_len;
			m->m_pkthdr.len = tot_len;
		} else {
			m_adj(m, tot_len - m->m_pkthdr.len);
		}
	}
}
static ip6_check_if_result_t
ip6_input_check_interface(struct mbuf *m, struct ip6_hdr *ip6, struct ifnet *inifp, struct route_in6 *rin6, struct ifnet **deliverifp)
{
	struct in6_ifaddr *__single ia6 = NULL;
	struct in6_addr tmp_dst = ip6->ip6_dst; /* copy to avoid unaligned access */
	struct in6_ifaddr *__single best_ia6 = NULL;
	uint32_t dst_ifscope = IFSCOPE_NONE;
	ip6_check_if_result_t result = IP6_CHECK_IF_NONE;

	*deliverifp = NULL;

	if (m->m_pkthdr.pkt_flags & PKTF_IFAINFO) {
		dst_ifscope = m->m_pkthdr.dst_ifindex;
	} else {
		dst_ifscope = inifp->if_index;
	}
	/*
	 * Check for exact addresses in the hash bucket.
	 */
	lck_rw_lock_shared(&in6_ifaddr_rwlock);
	TAILQ_FOREACH(ia6, IN6ADDR_HASH(&tmp_dst), ia6_hash) {
		/*
		 * TODO: should we accept loopback
		 */
		if (in6_are_addr_equal_scoped(&ia6->ia_addr.sin6_addr, &tmp_dst, ia6->ia_ifp->if_index, dst_ifscope)) {
			if ((ia6->ia6_flags & (IN6_IFF_NOTREADY | IN6_IFF_CLAT46))) {
				continue;
			}
			best_ia6 = ia6;
			if (ia6->ia_ifp == inifp) {
				/*
				 * TODO: should we also accept locally originated packets
				 * or from loopback ???
				 */
				break;
			}
			/*
			 * Continue the loop in case there's a exact match with another
			 * interface
			 */
		}
	}
	if (best_ia6 != NULL) {
		if (best_ia6->ia_ifp != inifp && ip6_forwarding == 0 &&
		    ((ip6_checkinterface == IP6_CHECKINTERFACE_HYBRID_ES &&
		    (best_ia6->ia_ifp->if_family == IFNET_FAMILY_IPSEC ||
		    best_ia6->ia_ifp->if_family == IFNET_FAMILY_UTUN)) ||
		    ip6_checkinterface == IP6_CHECKINTERFACE_STRONG_ES)) {
			/*
			 * Drop when interface address check is strict and forwarding
			 * is disabled
			 */
			result = IP6_CHECK_IF_DROP;
		} else {
			result = IP6_CHECK_IF_OURS;
			*deliverifp = best_ia6->ia_ifp;
			ip6_setdstifaddr_info(m, 0, best_ia6);
			ip6_setsrcifaddr_info(m, best_ia6->ia_ifp->if_index, NULL);
		}
	}
	lck_rw_done(&in6_ifaddr_rwlock);

	if (result == IP6_CHECK_IF_NONE) {
		/*
		 * Slow path: route lookup.
		 */
		struct sockaddr_in6 *__single dst6;

		dst6 = SIN6(&rin6->ro_dst);
		dst6->sin6_len = sizeof(struct sockaddr_in6);
		dst6->sin6_family = AF_INET6;
		dst6->sin6_addr = ip6->ip6_dst;
		if (!in6_embedded_scope && IN6_IS_SCOPE_EMBED(&ip6->ip6_dst)) {
			dst6->sin6_scope_id = dst_ifscope;
		}
		rtalloc_scoped_ign((struct route *)rin6,
		    RTF_PRCLONING, IFSCOPE_NONE);
		if (rin6->ro_rt != NULL) {
			RT_LOCK_SPIN(rin6->ro_rt);
		}

#define rt6_key(r) (SIN6(rn_get_key((r)->rt_nodes)))

		/*
		 * Accept the packet if the forwarding interface to the destination
		 * according to the routing table is the loopback interface,
		 * unless the associated route has a gateway.
		 * Note that this approach causes to accept a packet if there is a
		 * route to the loopback interface for the destination of the packet.
		 * But we think it's even useful in some situations, e.g. when using
		 * a special daemon which wants to intercept the packet.
		 *
		 * XXX: some OSes automatically make a cloned route for the destination
		 * of an outgoing packet.  If the outgoing interface of the packet
		 * is a loopback one, the kernel would consider the packet to be
		 * accepted, even if we have no such address assinged on the interface.
		 * We check the cloned flag of the route entry to reject such cases,
		 * assuming that route entries for our own addresses are not made by
		 * cloning (it should be true because in6_addloop explicitly installs
		 * the host route).  However, we might have to do an explicit check
		 * while it would be less efficient.  Or, should we rather install a
		 * reject route for such a case?
		 */
		if (rin6->ro_rt != NULL &&
		    (rin6->ro_rt->rt_flags & (RTF_HOST | RTF_GATEWAY)) == RTF_HOST &&
#if RTF_WASCLONED
		    !(rin6->ro_rt->rt_flags & RTF_WASCLONED) &&
#endif
		    rin6->ro_rt->rt_ifp->if_type == IFT_LOOP) {
			ia6 = ifatoia6(rin6->ro_rt->rt_ifa);
			/*
			 * Packets to a tentative, duplicated, or somehow invalid
			 * address must not be accepted.
			 *
			 * For performance, test without acquiring the address lock;
			 * a lot of things in the address are set once and never
			 * changed (e.g. ia_ifp.)
			 */
			if (!(ia6->ia6_flags & IN6_IFF_NOTREADY)) {
				/* this address is ready */
				result = IP6_CHECK_IF_OURS;
				*deliverifp = ia6->ia_ifp;       /* correct? */
				/*
				 * record dst address information into mbuf.
				 */
				(void) ip6_setdstifaddr_info(m, 0, ia6);
				(void) ip6_setsrcifaddr_info(m, ia6->ia_ifp->if_index, NULL);
			}
		}

		if (rin6->ro_rt != NULL) {
			RT_UNLOCK(rin6->ro_rt);
		}
	}

	if (result == IP6_CHECK_IF_NONE) {
		if (ip6_forwarding == 0) {
			result = IP6_CHECK_IF_DROP;
		} else {
			result = IP6_CHECK_IF_FORWARD;
			ip6_setdstifaddr_info(m, inifp->if_index, NULL);
			ip6_setsrcifaddr_info(m, inifp->if_index, NULL);
		}
	}

	if (result == IP6_CHECK_IF_OURS && *deliverifp != inifp) {
		ASSERT(*deliverifp != NULL);
		ip6stat.ip6s_rcv_if_weak_match++;

		/*  Logging is too noisy when forwarding is enabled */
		if (ip6_checkinterface_debug != IP6_CHECKINTERFACE_WEAK_ES && ip6_forwarding != 0) {
			char src_str[MAX_IPv6_STR_LEN];
			char dst_str[MAX_IPv6_STR_LEN];

			inet_ntop(AF_INET6, &ip6->ip6_src, src_str, sizeof(src_str));
			inet_ntop(AF_INET6, &ip6->ip6_dst, dst_str, sizeof(dst_str));
			os_log_info(OS_LOG_DEFAULT,
			    "%s: weak ES interface match to %s for packet from %s to %s proto %u received via %s",
			    __func__, (*deliverifp)->if_xname, src_str, dst_str, ip6->ip6_nxt, inifp->if_xname);
		}
	} else if (result == IP6_CHECK_IF_DROP) {
		ip6stat.ip6s_rcv_if_no_match++;
		if (ip6_checkinterface_debug > 0) {
			char src_str[MAX_IPv6_STR_LEN];
			char dst_str[MAX_IPv6_STR_LEN];

			inet_ntop(AF_INET6, &ip6->ip6_src, src_str, sizeof(src_str));
			inet_ntop(AF_INET6, &ip6->ip6_dst, dst_str, sizeof(dst_str));
			os_log(OS_LOG_DEFAULT,
			    "%s: no interface match for packet from %s to %s proto %u received via %s",
			    __func__, src_str, dst_str, ip6->ip6_nxt, inifp->if_xname);
		}
	}

	return result;
}

void
ip6_input(struct mbuf *m)
{
	struct ip6_hdr *ip6;
	int off = sizeof(struct ip6_hdr), nest;
	u_int32_t plen;
	u_int32_t rtalert = ~0;
	int nxt = 0, ours = 0;
	ifnet_ref_t inifp, deliverifp = NULL;
	ipfilter_t __single inject_ipfref = NULL;
	int seen = 1;
#if DUMMYNET
	struct m_tag *__single tag;
	struct ip_fw_args args = {};
#endif /* DUMMYNET */
	struct route_in6 rin6 = {};

	/*
	 * Check if the packet we received is valid after interface filter
	 * processing
	 */
	MBUF_INPUT_CHECK(m, m->m_pkthdr.rcvif);
	inifp = m->m_pkthdr.rcvif;
	VERIFY(inifp != NULL);

	/* Perform IP header alignment fixup, if needed */
	IP6_HDR_ALIGNMENT_FIXUP(m, inifp, return );

	m->m_pkthdr.pkt_flags &= ~PKTF_FORWARDED;
#if IPSEC
	/*
	 * should the inner packet be considered authentic?
	 * see comment in ah4_input().
	 */
	m->m_flags &= ~M_AUTHIPHDR;
	m->m_flags &= ~M_AUTHIPDGM;
#endif /* IPSEC */

	/*
	 * make sure we don't have onion peering information into m_aux.
	 */
	ip6_delaux(m);

#if DUMMYNET
	if ((tag = m_tag_locate(m, KERNEL_MODULE_TAG_ID,
	    KERNEL_TAG_TYPE_DUMMYNET)) != NULL) {
		struct dn_pkt_tag *__single dn_tag;

		dn_tag = (struct dn_pkt_tag *)(tag->m_tag_data);

		args.fwa_pf_rule = dn_tag->dn_pf_rule;

		m_tag_delete(m, tag);
	}

	if (args.fwa_pf_rule) {
		ip6 = mtod(m, struct ip6_hdr *); /* In case PF got disabled */

		goto check_with_pf;
	}
#endif /* DUMMYNET */

	/*
	 * No need to process packet twice if we've already seen it.
	 */
	inject_ipfref = ipf_get_inject_filter(m);
	if (inject_ipfref != NULL) {
		ip6 = mtod(m, struct ip6_hdr *);
		nxt = ip6->ip6_nxt;
		seen = 0;
		goto injectit;
	} else {
		seen = 1;
	}

	if (__improbable(m->m_pkthdr.pkt_flags & PKTF_WAKE_PKT)) {
		if_ports_used_match_mbuf(inifp, PF_INET6, m);
	}

	/*
	 * mbuf statistics
	 */
	if (m->m_flags & M_EXT) {
		if (m->m_next != NULL) {
			ip6stat.ip6s_mext2m++;
		} else {
			ip6stat.ip6s_mext1++;
		}
	} else {
#define M2MMAX  (sizeof (ip6stat.ip6s_m2m) / sizeof (ip6stat.ip6s_m2m[0]))
		if (m->m_next != NULL) {
			if (m->m_pkthdr.pkt_flags & PKTF_LOOP) {
				/* XXX */
				ip6stat.ip6s_m2m[ifnet_index(lo_ifp)]++;
			} else if (inifp->if_index < M2MMAX) {
				ip6stat.ip6s_m2m[inifp->if_index]++;
			} else {
				ip6stat.ip6s_m2m[0]++;
			}
		} else {
			ip6stat.ip6s_m1++;
		}
#undef M2MMAX
	}

	/*
	 * Drop the packet if IPv6 operation is disabled on the interface.
	 */
	if (inifp->if_eflags & IFEF_IPV6_DISABLED) {
		m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP6_IF_IPV6_DISABLED, NULL, 0);
		goto bad;
	}

	in6_ifstat_inc_na(inifp, ifs6_in_receive);
	ip6stat.ip6s_total++;

	/*
	 * L2 bridge code and some other code can return mbuf chain
	 * that does not conform to KAME requirement.  too bad.
	 * XXX: fails to join if interface MTU > MCLBYTES.  jumbogram?
	 */
	if (m->m_next != NULL && m->m_pkthdr.len < MCLBYTES) {
		mbuf_ref_t n;

		MGETHDR(n, M_DONTWAIT, MT_HEADER);      /* MAC-OK */
		if (n) {
			M_COPY_PKTHDR(n, m);
		}
		if (n && m->m_pkthdr.len > MHLEN) {
			MCLGET(n, M_DONTWAIT);
			if ((n->m_flags & M_EXT) == 0) {
				m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_TOO_SMALL, NULL, 0);
				n = NULL;
			}
		}
		if (n == NULL) {
			goto bad;
		}

		m_copydata(m, 0, m->m_pkthdr.len, mtod(n, caddr_t));
		n->m_len = m->m_pkthdr.len;
		m_freem(m);
		m = n;
	}
	IP6_EXTHDR_CHECK(m, 0, sizeof(struct ip6_hdr), { goto done; });

	if (m->m_len < sizeof(struct ip6_hdr)) {
		if ((m = m_pullup(m, sizeof(struct ip6_hdr))) == 0) {
			ip6stat.ip6s_toosmall++;
			in6_ifstat_inc(inifp, ifs6_in_hdrerr);
			goto done;
		}
	}

	ip6 = mtod(m, struct ip6_hdr *);

	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
		ip6stat.ip6s_badvers++;
		in6_ifstat_inc(inifp, ifs6_in_hdrerr);
		m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_BAD_VERSION, NULL, 0);
		goto bad;
	}

	ip6stat.ip6s_nxthist[ip6->ip6_nxt]++;

	/*
	 * Check against address spoofing/corruption.
	 */
	if (!(m->m_pkthdr.pkt_flags & PKTF_LOOP) &&
	    IN6_IS_ADDR_LOOPBACK(&ip6->ip6_src)) {
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(inifp, ifs6_in_addrerr);
		m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP6_BAD_SCOPE, NULL, 0);
		goto bad;
	}
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_src) ||
	    IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_dst)) {
		/*
		 * XXX: "badscope" is not very suitable for a multicast source.
		 */
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(inifp, ifs6_in_addrerr);
		m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP6_BAD_SCOPE, NULL, 0);
		goto bad;
	}
	if (IN6_IS_ADDR_MC_INTFACELOCAL(&ip6->ip6_dst) &&
	    !(m->m_pkthdr.pkt_flags & PKTF_LOOP)) {
		/*
		 * In this case, the packet should come from the loopback
		 * interface.  However, we cannot just check the if_flags,
		 * because ip6_mloopback() passes the "actual" interface
		 * as the outgoing/incoming interface.
		 */
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(inifp, ifs6_in_addrerr);
		m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP6_BAD_SCOPE, NULL, 0);
		goto bad;
	}

	/*
	 * The following check is not documented in specs.  A malicious
	 * party may be able to use IPv4 mapped addr to confuse tcp/udp stack
	 * and bypass security checks (act as if it was from 127.0.0.1 by using
	 * IPv6 src ::ffff:127.0.0.1).  Be cautious.
	 *
	 * This check chokes if we are in an SIIT cloud.  As none of BSDs
	 * support IPv4-less kernel compilation, we cannot support SIIT
	 * environment at all.  So, it makes more sense for us to reject any
	 * malicious packets for non-SIIT environment, than try to do a
	 * partial support for SIIT environment.
	 */
	if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(inifp, ifs6_in_addrerr);
		m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP6_BAD_SCOPE, NULL, 0);
		goto bad;
	}

	if (((ntohl(ip6->ip6_flow & IPV6_FLOW_ECN_MASK) >> 20) & IPTOS_ECN_ECT1) == IPTOS_ECN_ECT1) {
		m->m_pkthdr.pkt_ext_flags |= PKTF_EXT_L4S;
	}

#if 0
	/*
	 * Reject packets with IPv4 compatible addresses (auto tunnel).
	 *
	 * The code forbids auto tunnel relay case in RFC1933 (the check is
	 * stronger than RFC1933).  We may want to re-enable it if mech-xx
	 * is revised to forbid relaying case.
	 */
	if (IN6_IS_ADDR_V4COMPAT(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4COMPAT(&ip6->ip6_dst)) {
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(inifp, ifs6_in_addrerr);
		m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP6_BAD_SCOPE, NULL, 0);
		goto bad;
	}
#endif

	/*
	 * Naively assume we can attribute inbound data to the route we would
	 * use to send to this destination. Asymetric routing breaks this
	 * assumption, but it still allows us to account for traffic from
	 * a remote node in the routing table.
	 * this has a very significant performance impact so we bypass
	 * if nstat_collect is disabled. We may also bypass if the
	 * protocol is tcp in the future because tcp will have a route that
	 * we can use to attribute the data to. That does mean we would not
	 * account for forwarded tcp traffic.
	 */
	if (nstat_collect) {
		rtentry_ref_t rte =
		    ifnet_cached_rtlookup_inet6(inifp, &ip6->ip6_src);
		if (rte != NULL) {
			nstat_route_rx(rte, 1, m->m_pkthdr.len, 0);
			rtfree(rte);
		}
	}

#if DUMMYNET
check_with_pf:
#endif /* DUMMYNET */
#if PF
	/* Invoke inbound packet filter */
	if (PF_IS_ENABLED) {
		int error;
#if DUMMYNET
		error = pf_af_hook(inifp, NULL, &m, AF_INET6, TRUE, &args);
#else /* !DUMMYNET */
		error = pf_af_hook(inifp, NULL, &m, AF_INET6, TRUE, NULL);
#endif /* !DUMMYNET */
		if (error != 0 || m == NULL) {
			if (m != NULL) {
				panic("%s: unexpected packet %p",
				    __func__, m);
				/* NOTREACHED */
			}
			/* Already freed by callee */
			goto done;
		}
		ip6 = mtod(m, struct ip6_hdr *);
	}
#endif /* PF */

	/* drop packets if interface ID portion is already filled */
	if (!(inifp->if_flags & IFF_LOOPBACK) &&
	    !(m->m_pkthdr.pkt_flags & PKTF_LOOP)) {
		if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_src) &&
		    ip6->ip6_src.s6_addr16[1]) {
			ip6stat.ip6s_badscope++;
			m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP6_BAD_SCOPE, NULL, 0);
			goto bad;
		}
		if (IN6_IS_SCOPE_EMBED(&ip6->ip6_dst) &&
		    ip6->ip6_dst.s6_addr16[1]) {
			ip6stat.ip6s_badscope++;
			m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP6_BAD_SCOPE, NULL, 0);
			goto bad;
		}
	}

	if ((m->m_pkthdr.pkt_flags & PKTF_IFAINFO) != 0 && in6_embedded_scope) {
		if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_src)) {
			ip6->ip6_src.s6_addr16[1] =
			    htons(m->m_pkthdr.src_ifindex);
		}
		if (IN6_IS_SCOPE_EMBED(&ip6->ip6_dst)) {
			ip6->ip6_dst.s6_addr16[1] =
			    htons(m->m_pkthdr.dst_ifindex);
		}
	} else if (in6_embedded_scope) {
		if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_src)) {
			ip6->ip6_src.s6_addr16[1] = htons(inifp->if_index);
		}
		if (IN6_IS_SCOPE_EMBED(&ip6->ip6_dst)) {
			ip6->ip6_dst.s6_addr16[1] = htons(inifp->if_index);
		}
	}

	/*
	 * Multicast check
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
		struct in6_multi *__single in6m = NULL;

		in6_ifstat_inc_na(inifp, ifs6_in_mcast);
		/*
		 * See if we belong to the destination multicast group on the
		 * arrival interface.
		 */
		in6_multihead_lock_shared();
		IN6_LOOKUP_MULTI(&ip6->ip6_dst, inifp, in6m);
		in6_multihead_lock_done();
		if (in6m != NULL) {
			IN6M_REMREF(in6m);
			ours = 1;
		} else if (!nd6_prproxy) {
			ip6stat.ip6s_notmember++;
			ip6stat.ip6s_cantforward++;
			in6_ifstat_inc(inifp, ifs6_in_discard);
			m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_UNKNOWN_MULTICAST_GROUP, NULL, 0);
			goto bad;
		}
		deliverifp = inifp;
		/*
		 * record dst address information into mbuf, if we don't have one yet.
		 * note that we are unable to record it, if the address is not listed
		 * as our interface address (e.g. multicast addresses, etc.)
		 */
		if (deliverifp != NULL) {
			struct in6_ifaddr *__single ia6 = NULL;

			ia6 = in6_ifawithifp(deliverifp, &ip6->ip6_dst);
			if (ia6 != NULL) {
				(void) ip6_setdstifaddr_info(m, 0, ia6);
				(void) ip6_setsrcifaddr_info(m, ia6->ia_ifp->if_index, NULL);
				ifa_remref(&ia6->ia_ifa);
			} else {
				(void) ip6_setdstifaddr_info(m, inifp->if_index, NULL);
				(void) ip6_setsrcifaddr_info(m, inifp->if_index, NULL);
			}
		}
		goto hbhcheck;
	} else {
		/*
		 * Unicast check
		 */
		ip6_check_if_result_t check_if_result = IP6_CHECK_IF_NONE;
		check_if_result = ip6_input_check_interface(m, ip6, inifp, &rin6, &deliverifp);
		ASSERT(check_if_result != IP6_CHECK_IF_NONE);
		if (check_if_result == IP6_CHECK_IF_OURS) {
			ours = 1;
			goto hbhcheck;
		} else if (check_if_result == IP6_CHECK_IF_DROP) {
			m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_RCV_IF_NO_MATCH, NULL, 0);
			goto bad;
		}
	}

	/*
	 * Now there is no reason to process the packet if it's not our own
	 * and we're not a router.
	 */
	if (!ip6_forwarding) {
		ip6stat.ip6s_cantforward++;
		in6_ifstat_inc(inifp, ifs6_in_discard);
		/*
		 * Raise a kernel event if the packet received on cellular
		 * interface is not intended for local host.
		 * For now limit it to ICMPv6 packets.
		 */
		if (inifp->if_type == IFT_CELLULAR &&
		    ip6->ip6_nxt == IPPROTO_ICMPV6) {
			in6_ifstat_inc(inifp, ifs6_cantfoward_icmp6);
		}
		m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_CANNOT_FORWARD, NULL, 0);
		goto bad;
	}

hbhcheck:
	/*
	 * Process Hop-by-Hop options header if it's contained.
	 * m may be modified in ip6_hopopts_input().
	 * If a JumboPayload option is included, plen will also be modified.
	 */
	plen = (u_int32_t)ntohs(ip6->ip6_plen);
	if (ip6->ip6_nxt == IPPROTO_HOPOPTS) {
		struct ip6_hbh *__single hbh;

		/*
		 * Mark the packet to imply that HBH option has been checked.
		 * This can only be true is the packet came in unfragmented
		 * or if the option is in the first fragment
		 */
		m->m_pkthdr.pkt_flags |= PKTF_HBH_CHKED;
		if (ip6_hopopts_input(&plen, &rtalert, &m, &off)) {
#if 0   /* touches NULL pointer */
			in6_ifstat_inc(inifp, ifs6_in_discard);
#endif
			goto done;      /* m have already been freed */
		}

		/* adjust pointer */
		ip6 = mtod(m, struct ip6_hdr *);

		/*
		 * if the payload length field is 0 and the next header field
		 * indicates Hop-by-Hop Options header, then a Jumbo Payload
		 * option MUST be included.
		 */
		if (ip6->ip6_plen == 0 && plen == 0) {
			/*
			 * Note that if a valid jumbo payload option is
			 * contained, ip6_hopopts_input() must set a valid
			 * (non-zero) payload length to the variable plen.
			 */
			ip6stat.ip6s_badoptions++;
			in6_ifstat_inc(inifp, ifs6_in_discard);
			in6_ifstat_inc(inifp, ifs6_in_hdrerr);
			icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
			    (int)((caddr_t)&ip6->ip6_plen - (caddr_t)ip6));
			goto done;
		}
		/* ip6_hopopts_input() ensures that mbuf is contiguous */
		hbh = (struct ip6_hbh *)(ip6 + 1);
		nxt = hbh->ip6h_nxt;

		/*
		 * If we are acting as a router and the packet contains a
		 * router alert option, see if we know the option value.
		 * Currently, we only support the option value for MLD, in which
		 * case we should pass the packet to the multicast routing
		 * daemon.
		 */
		if (rtalert != ~0 && ip6_forwarding) {
			switch (rtalert) {
			case IP6OPT_RTALERT_MLD:
				ours = 1;
				break;
			default:
				/*
				 * RFC2711 requires unrecognized values must be
				 * silently ignored.
				 */
				break;
			}
		}
	} else {
		nxt = ip6->ip6_nxt;
	}

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IPv6 header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len - sizeof(struct ip6_hdr) < plen) {
		ip6stat.ip6s_tooshort++;
		in6_ifstat_inc(inifp, ifs6_in_truncated);
		m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_TOO_SHORT, NULL, 0);
		goto bad;
	}

	if (m->m_pkthdr.len > sizeof(struct ip6_hdr) + plen) {
		ip6_input_adjust(m, ip6, plen, inifp);
	}

	/*
	 * Forward if desirable.
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
		if (!ours && nd6_prproxy) {
			/*
			 * If this isn't for us, this might be a Neighbor
			 * Solicitation (dst is solicited-node multicast)
			 * against an address in one of the proxied prefixes;
			 * if so, claim the packet and let icmp6_input()
			 * handle the rest.
			 */
			ours = nd6_prproxy_isours(m, ip6, NULL, IFSCOPE_NONE);
			VERIFY(!ours ||
			    (m->m_pkthdr.pkt_flags & PKTF_PROXY_DST));
		}
		if (!ours) {
			m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP6_UNPROXIED_NS, NULL, 0);
			goto bad;
		}
	} else if (!ours) {
		/*
		 * The unicast forwarding function might return the packet
		 * if we are proxying prefix(es), and if the packet is an
		 * ICMPv6 packet that has failed the zone checks, but is
		 * targetted towards a proxied address (this is optimized by
		 * way of RTF_PROXY test.)  If so, claim the packet as ours
		 * and let icmp6_input() handle the rest.  The packet's hop
		 * limit value is kept intact (it's not decremented).  This
		 * is for supporting Neighbor Unreachability Detection between
		 * proxied nodes on different links (src is link-local, dst
		 * is target address.)
		 */
		if ((m = ip6_forward(m, &rin6, 0)) == NULL) {
			goto done;
		}
		VERIFY(rin6.ro_rt != NULL);
		VERIFY(m->m_pkthdr.pkt_flags & PKTF_PROXY_DST);
		deliverifp = rin6.ro_rt->rt_ifp;
		ours = 1;
	}

	ip6 = mtod(m, struct ip6_hdr *);

	/*
	 * Malicious party may be able to use IPv4 mapped addr to confuse
	 * tcp/udp stack and bypass security checks (act as if it was from
	 * 127.0.0.1 by using IPv6 src ::ffff:127.0.0.1).  Be cautious.
	 *
	 * For SIIT end node behavior, you may want to disable the check.
	 * However, you will  become vulnerable to attacks using IPv4 mapped
	 * source.
	 */
	if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
		ip6stat.ip6s_badscope++;
		in6_ifstat_inc(inifp, ifs6_in_addrerr);
		m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP6_BAD_SCOPE, NULL, 0);
		goto bad;
	}

	/*
	 * Tell launch routine the next header
	 */
	ip6stat.ip6s_delivered++;
	in6_ifstat_inc_na(deliverifp, ifs6_in_deliver);

injectit:
	nest = 0;

	/*
	 * Perform IP header alignment fixup again, if needed.  Note that
	 * we do it once for the outermost protocol, and we assume each
	 * protocol handler wouldn't mess with the alignment afterwards.
	 */
	IP6_HDR_ALIGNMENT_FIXUP(m, inifp, return );

	while (nxt != IPPROTO_DONE) {
		struct ipfilter *__single filter;
		int (*pr_input)(struct mbuf **, int *, int);

		/*
		 * This would imply either IPPROTO_HOPOPTS was not the first
		 * option or it did not come in the first fragment.
		 */
		if (nxt == IPPROTO_HOPOPTS &&
		    (m->m_pkthdr.pkt_flags & PKTF_HBH_CHKED) == 0) {
			/*
			 * This implies that HBH option was not contained
			 * in the first fragment
			 */
			ip6stat.ip6s_badoptions++;
			m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP6_BAD_OPTION, NULL, 0);
			goto bad;
		}

		if (ip6_hdrnestlimit && (++nest > ip6_hdrnestlimit)) {
			ip6stat.ip6s_toomanyhdr++;
			m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP6_TOO_MANY_OPTIONS, NULL, 0);
			goto bad;
		}

		/*
		 * protection against faulty packet - there should be
		 * more sanity checks in header chain processing.
		 */
		if (m->m_pkthdr.len < off) {
			ip6stat.ip6s_tooshort++;
			in6_ifstat_inc(inifp, ifs6_in_truncated);
			m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_TOO_SHORT, NULL, 0);
			goto bad;
		}

#if IPSEC
		/*
		 * enforce IPsec policy checking if we are seeing last header.
		 * note that we do not visit this with protocols with pcb layer
		 * code - like udp/tcp/raw ip.
		 */
		if ((ipsec_bypass == 0) &&
		    (ip6_protox[nxt]->pr_flags & PR_LASTHDR) != 0) {
			if (ipsec6_in_reject(m, NULL)) {
				IPSEC_STAT_INCREMENT(ipsec6stat.in_polvio);
				m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IPSEC_REJECT, NULL, 0);
				goto bad;
			}
		}
#endif /* IPSEC */

		/*
		 * Call IP filter
		 */
		if (!TAILQ_EMPTY(&ipv6_filters) && !IFNET_IS_INTCOPROC(inifp)) {
			ipf_ref();
			TAILQ_FOREACH(filter, &ipv6_filters, ipf_link) {
				if (seen == 0) {
					if ((struct ipfilter *)inject_ipfref ==
					    filter) {
						seen = 1;
					}
				} else if (filter->ipf_filter.ipf_input) {
					errno_t result;

					result = filter->ipf_filter.ipf_input(
						filter->ipf_filter.cookie,
						(mbuf_t *)&m, off, (uint8_t)nxt);
					if (result == EJUSTRETURN) {
						ipf_unref();
						goto done;
					}
					if (result != 0) {
						ipf_unref();
						m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_FILTER_DROP, NULL, 0);
						goto bad;
					}
				}
			}
			ipf_unref();
		}

		DTRACE_IP6(receive, struct mbuf *, m, struct inpcb *, NULL,
		    struct ip6_hdr *, ip6, struct ifnet *, inifp,
		    struct ip *, NULL, struct ip6_hdr *, ip6);

		if ((pr_input = ip6_protox[nxt]->pr_input) == NULL) {
			m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_NO_PROTO, NULL, 0);
			m = NULL;
			nxt = IPPROTO_DONE;
		} else if (!(ip6_protox[nxt]->pr_flags & PR_PROTOLOCK)) {
			lck_mtx_lock(inet6_domain_mutex);
			nxt = pr_input(&m, &off, nxt);
			lck_mtx_unlock(inet6_domain_mutex);
		} else {
			nxt = pr_input(&m, &off, nxt);
		}
	}
done:
	ROUTE_RELEASE(&rin6);
	return;
bad:
	goto done;
}

void
ip6_setsrcifaddr_info(struct mbuf *m, uint32_t src_idx, struct in6_ifaddr *ia6)
{
	VERIFY(m->m_flags & M_PKTHDR);
	m->m_pkthdr.pkt_ext_flags &= ~PKTF_EXT_OUTPUT_SCOPE;
	/*
	 * If the source ifaddr is specified, pick up the information
	 * from there; otherwise just grab the passed-in ifindex as the
	 * caller may not have the ifaddr available.
	 */
	if (ia6 != NULL) {
		m->m_pkthdr.pkt_flags |= PKTF_IFAINFO;
		m->m_pkthdr.src_ifindex = ia6->ia_ifp->if_index;

		/* See IN6_IFF comments in in6_var.h */
		m->m_pkthdr.src_iff = (ia6->ia6_flags & 0xffff);
	} else {
		m->m_pkthdr.src_iff = 0;
		m->m_pkthdr.src_ifindex = (uint16_t)src_idx;
		if (src_idx != 0) {
			m->m_pkthdr.pkt_flags |= PKTF_IFAINFO;
		}
	}
}

void
ip6_setdstifaddr_info(struct mbuf *m, uint32_t dst_idx, struct in6_ifaddr *ia6)
{
	VERIFY(m->m_flags & M_PKTHDR);
	m->m_pkthdr.pkt_ext_flags &= ~PKTF_EXT_OUTPUT_SCOPE;

	/*
	 * If the destination ifaddr is specified, pick up the information
	 * from there; otherwise just grab the passed-in ifindex as the
	 * caller may not have the ifaddr available.
	 */
	if (ia6 != NULL) {
		m->m_pkthdr.pkt_flags |= PKTF_IFAINFO;
		m->m_pkthdr.dst_ifindex = ia6->ia_ifp->if_index;

		/* See IN6_IFF comments in in6_var.h */
		m->m_pkthdr.dst_iff = (ia6->ia6_flags & 0xffff);
	} else {
		m->m_pkthdr.dst_iff = 0;
		m->m_pkthdr.dst_ifindex = (uint16_t)dst_idx;
		if (dst_idx != 0) {
			m->m_pkthdr.pkt_flags |= PKTF_IFAINFO;
		}
	}
}

int
ip6_getsrcifaddr_info(struct mbuf *m, uint32_t *src_idx, uint32_t *ia6f)
{
	VERIFY(m->m_flags & M_PKTHDR);

	if (!(m->m_pkthdr.pkt_flags & PKTF_IFAINFO)) {
		return -1;
	}

	if (src_idx != NULL) {
		*src_idx = m->m_pkthdr.src_ifindex;
	}

	if (ia6f != NULL) {
		*ia6f = m->m_pkthdr.src_iff;
	}

	return 0;
}

int
ip6_getdstifaddr_info(struct mbuf *m, uint32_t *dst_idx, uint32_t *ia6f)
{
	VERIFY(m->m_flags & M_PKTHDR);

	if (!(m->m_pkthdr.pkt_flags & PKTF_IFAINFO)) {
		return -1;
	}

	if (dst_idx != NULL) {
		*dst_idx = m->m_pkthdr.dst_ifindex;
	}

	if (ia6f != NULL) {
		*ia6f = m->m_pkthdr.dst_iff;
	}

	return 0;
}

uint32_t
ip6_input_getsrcifscope(struct mbuf *m)
{
	VERIFY(m->m_flags & M_PKTHDR);

	if (m->m_pkthdr.rcvif != NULL) {
		return m->m_pkthdr.rcvif->if_index;
	}

	uint32_t src_ifscope = IFSCOPE_NONE;
	ip6_getsrcifaddr_info(m, &src_ifscope, NULL);
	return src_ifscope;
}

uint32_t
ip6_input_getdstifscope(struct mbuf *m)
{
	VERIFY(m->m_flags & M_PKTHDR);

	if (m->m_pkthdr.rcvif != NULL) {
		return m->m_pkthdr.rcvif->if_index;
	}

	uint32_t dst_ifscope = IFSCOPE_NONE;
	ip6_getdstifaddr_info(m, &dst_ifscope, NULL);
	return dst_ifscope;
}

/*
 * Hop-by-Hop options header processing. If a valid jumbo payload option is
 * included, the real payload length will be stored in plenp.
 */
static int
ip6_hopopts_input(uint32_t *plenp, uint32_t *rtalertp, struct mbuf **mp,
    int *offp)
{
	mbuf_ref_t m = *mp;
	int off = *offp, hbhlen;
	struct ip6_hbh *hbh;
	u_int8_t *opt;

	/* validation of the length of the header */
	IP6_EXTHDR_CHECK(m, off, sizeof(*hbh), return (-1));
	hbh = (struct ip6_hbh *)(mtod(m, caddr_t) + off);
	hbhlen = (hbh->ip6h_len + 1) << 3;

	IP6_EXTHDR_CHECK(m, off, hbhlen, return (-1));
	hbh = (struct ip6_hbh *)(mtod(m, caddr_t) + off);
	off += hbhlen;
	hbhlen -= sizeof(struct ip6_hbh);
	opt = (u_int8_t *)hbh + sizeof(struct ip6_hbh);

	if (ip6_process_hopopts(m, (u_int8_t *)hbh + sizeof(struct ip6_hbh),
	    hbhlen, rtalertp, plenp) < 0) {
		return -1;
	}

	*offp = off;
	*mp = m;
	return 0;
}

/*
 * Search header for all Hop-by-hop options and process each option.
 * This function is separate from ip6_hopopts_input() in order to
 * handle a case where the sending node itself process its hop-by-hop
 * options header. In such a case, the function is called from ip6_output().
 *
 * The function assumes that hbh header is located right after the IPv6 header
 * (RFC2460 p7), opthead is pointer into data content in m, and opthead to
 * opthead + hbhlen is located in continuous memory region.
 */
int
ip6_process_hopopts(struct mbuf *m, u_int8_t *__sized_by(inhbhlen)opthead, int inhbhlen,
    u_int32_t *rtalertp, u_int32_t *plenp)
{
	struct ip6_hdr *__single ip6;
	int optlen = 0;
	int hbhlen = inhbhlen;
	u_int8_t *opt = opthead;
	u_int16_t rtalert_val;
	u_int32_t jumboplen;
	const int erroff = sizeof(struct ip6_hdr) + sizeof(struct ip6_hbh);

	for (; hbhlen > 0; hbhlen -= optlen, opt += optlen) {
		switch (*opt) {
		case IP6OPT_PAD1:
			optlen = 1;
			break;
		case IP6OPT_PADN:
			if (hbhlen < IP6OPT_MINLEN) {
				ip6stat.ip6s_toosmall++;
				m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_TOO_SMALL, NULL, 0);
				goto bad;
			}
			optlen = *(opt + 1) + 2;
			break;
		case IP6OPT_ROUTER_ALERT:
			/* XXX may need check for alignment */
			if (hbhlen < IP6OPT_RTALERT_LEN) {
				ip6stat.ip6s_toosmall++;
				goto bad;
			}
			if (*(opt + 1) != IP6OPT_RTALERT_LEN - 2) {
				/* XXX stat */
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    (int)(erroff + opt + 1 - opthead));
				return -1;
			}
			optlen = IP6OPT_RTALERT_LEN;
			bcopy((caddr_t)(opt + 2), (caddr_t)&rtalert_val, 2);
			*rtalertp = ntohs(rtalert_val);
			break;
		case IP6OPT_JUMBO:
			/* XXX may need check for alignment */
			if (hbhlen < IP6OPT_JUMBO_LEN) {
				ip6stat.ip6s_toosmall++;
				m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_TOO_SMALL, NULL, 0);
				goto bad;
			}
			if (*(opt + 1) != IP6OPT_JUMBO_LEN - 2) {
				/* XXX stat */
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    (int)(erroff + opt + 1 - opthead));
				return -1;
			}
			optlen = IP6OPT_JUMBO_LEN;

			/*
			 * IPv6 packets that have non 0 payload length
			 * must not contain a jumbo payload option.
			 */
			ip6 = mtod(m, struct ip6_hdr *);
			if (ip6->ip6_plen) {
				ip6stat.ip6s_badoptions++;
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    (int)(erroff + opt - opthead));
				return -1;
			}

			/*
			 * We may see jumbolen in unaligned location, so
			 * we'd need to perform bcopy().
			 */
			bcopy(opt + 2, &jumboplen, sizeof(jumboplen));
			jumboplen = (u_int32_t)htonl(jumboplen);

#if 1
			/*
			 * if there are multiple jumbo payload options,
			 * *plenp will be non-zero and the packet will be
			 * rejected.
			 * the behavior may need some debate in ipngwg -
			 * multiple options does not make sense, however,
			 * there's no explicit mention in specification.
			 */
			if (*plenp != 0) {
				ip6stat.ip6s_badoptions++;
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    (int)(erroff + opt + 2 - opthead));
				return -1;
			}
#endif

			/*
			 * jumbo payload length must be larger than 65535.
			 */
			if (jumboplen <= IPV6_MAXPACKET) {
				ip6stat.ip6s_badoptions++;
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    (int)(erroff + opt + 2 - opthead));
				return -1;
			}
			*plenp = jumboplen;

			break;
		default:                /* unknown option */
			if (hbhlen < IP6OPT_MINLEN) {
				ip6stat.ip6s_toosmall++;
				m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP_TOO_SMALL, NULL, 0);
				goto bad;
			}
			optlen = ip6_unknown_opt(opt, hbhlen, m,
			    erroff + opt - opthead);
			if (optlen == -1) {
				return -1;
			}
			optlen += 2;
			break;
		}
	}

	return 0;

bad:
	return -1;
}

/*
 * Unknown option processing.
 * The fourth argument `off' is the offset from the IPv6 header to the option,
 * which is necessary if the IPv6 header the and option header and IPv6 header
 * is not continuous in order to return an ICMPv6 error.
 */
int
ip6_unknown_opt(uint8_t *__counted_by(optplen) optp, size_t optplen, struct mbuf *m, size_t off)
{
#pragma unused(optplen)

	struct ip6_hdr *__single ip6;

	switch (IP6OPT_TYPE(*optp)) {
	case IP6OPT_TYPE_SKIP: /* ignore the option */
		return (int)*(optp + 1);

	case IP6OPT_TYPE_DISCARD:       /* silently discard */
		m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP6_OPT_DISCARD, NULL, 0);
		return -1;

	case IP6OPT_TYPE_FORCEICMP: /* send ICMP even if multicasted */
		ip6stat.ip6s_badoptions++;
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_OPTION, (int)off);
		return -1;

	case IP6OPT_TYPE_ICMP: /* send ICMP if not multicasted */
		ip6stat.ip6s_badoptions++;
		ip6 = mtod(m, struct ip6_hdr *);
		if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
		    (m->m_flags & (M_BCAST | M_MCAST))) {
			m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_IP6_BAD_OPTION, NULL, 0);
		} else {
			icmp6_error(m, ICMP6_PARAM_PROB,
			    ICMP6_PARAMPROB_OPTION, (int)off);
		}
		return -1;
	}

	m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, DROP_REASON_UNSPECIFIED, NULL, 0); /* XXX: NOTREACHED */
	return -1;
}

/*
 * Create the "control" list for this pcb.
 * These functions will not modify mbuf chain at all.
 *
 * With KAME mbuf chain restriction:
 * The routine will be called from upper layer handlers like tcp6_input().
 * Thus the routine assumes that the caller (tcp6_input) have already
 * called IP6_EXTHDR_CHECK() and all the extension headers are located in the
 * very first mbuf on the mbuf chain.
 *
 * ip6_savecontrol_v4 will handle those options that are possible to be
 * set on a v4-mapped socket.
 * ip6_savecontrol will directly call ip6_savecontrol_v4 to handle those
 * options and handle the v6-only ones itself.
 */
struct mbuf **
ip6_savecontrol_v4(struct inpcb *inp, struct mbuf *m, struct mbuf **mp,
    int *v4only)
{
	struct ip6_hdr *__single ip6 = mtod(m, struct ip6_hdr *);

	if ((inp->inp_socket->so_options & SO_TIMESTAMP) != 0) {
		struct timeval tv;

		getmicrotime(&tv);
		mp = sbcreatecontrol_mbuf((caddr_t)&tv, sizeof(tv),
		    SCM_TIMESTAMP, SOL_SOCKET, mp);
		if (*mp == NULL) {
			return NULL;
		}
	}
	if ((inp->inp_socket->so_options & SO_TIMESTAMP_MONOTONIC) != 0) {
		uint64_t time;

		time = mach_absolute_time();
		mp = sbcreatecontrol_mbuf((caddr_t)&time, sizeof(time),
		    SCM_TIMESTAMP_MONOTONIC, SOL_SOCKET, mp);
		if (*mp == NULL) {
			return NULL;
		}
	}
	if ((inp->inp_socket->so_options & SO_TIMESTAMP_CONTINUOUS) != 0) {
		uint64_t time;

		time = mach_continuous_time();
		mp = sbcreatecontrol_mbuf((caddr_t)&time, sizeof(time),
		    SCM_TIMESTAMP_CONTINUOUS, SOL_SOCKET, mp);
		if (*mp == NULL) {
			return NULL;
		}
	}
	if ((inp->inp_socket->so_flags & SOF_RECV_TRAFFIC_CLASS) != 0) {
		int tc = m_get_traffic_class(m);

		mp = sbcreatecontrol_mbuf((caddr_t)&tc, sizeof(tc),
		    SO_TRAFFIC_CLASS, SOL_SOCKET, mp);
		if (*mp == NULL) {
			return NULL;
		}
	}

	if ((inp->inp_socket->so_flags & SOF_RECV_WAKE_PKT) &&
	    (m->m_pkthdr.pkt_flags & PKTF_WAKE_PKT)) {
		int flag = 1;

		mp = sbcreatecontrol_mbuf((caddr_t)&flag, sizeof(flag),
		    SO_RECV_WAKE_PKT, SOL_SOCKET, mp);
		if (*mp == NULL) {
			return NULL;
		}
	}

#define IS2292(inp, x, y)       (((inp)->inp_flags & IN6P_RFC2292) ? (x) : (y))
	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
		if (v4only != NULL) {
			*v4only = 1;
		}

		// Send ECN flags for v4-mapped addresses
		if ((inp->inp_flags & IN6P_TCLASS) != 0) {
			struct ip *__single ip_header = mtod(m, struct ip *);

			int tclass = (int)(ip_header->ip_tos);
			mp = sbcreatecontrol_mbuf((caddr_t)&tclass, sizeof(tclass),
			    IPV6_TCLASS, IPPROTO_IPV6, mp);
			if (*mp == NULL) {
				return NULL;
			}
		}

		// Send IN6P_PKTINFO for v4-mapped address
		if ((inp->inp_flags & IN6P_PKTINFO) != 0 || SOFLOW_ENABLED(inp->inp_socket)) {
			struct in6_pktinfo pi6 = {
				.ipi6_addr = IN6ADDR_V4MAPPED_INIT,
				.ipi6_ifindex = (m && m->m_pkthdr.rcvif) ? m->m_pkthdr.rcvif->if_index : 0,
			};

			struct ip *__single ip_header = mtod(m, struct ip *);
			bcopy(&ip_header->ip_dst, &pi6.ipi6_addr.s6_addr32[3], sizeof(struct in_addr));

			mp = sbcreatecontrol_mbuf((caddr_t)&pi6,
			    sizeof(struct in6_pktinfo),
			    IS2292(inp, IPV6_2292PKTINFO, IPV6_PKTINFO),
			    IPPROTO_IPV6, mp);
			if (*mp == NULL) {
				return NULL;
			}
		}
		return mp;
	}

	/* RFC 2292 sec. 5 */
	if ((inp->inp_flags & IN6P_PKTINFO) != 0 || SOFLOW_ENABLED(inp->inp_socket)) {
		struct in6_pktinfo pi6;

		bcopy(&ip6->ip6_dst, &pi6.ipi6_addr, sizeof(struct in6_addr));
		in6_clearscope(&pi6.ipi6_addr); /* XXX */
		pi6.ipi6_ifindex =
		    (m && m->m_pkthdr.rcvif) ? m->m_pkthdr.rcvif->if_index : 0;

		mp = sbcreatecontrol_mbuf((caddr_t)&pi6,
		    sizeof(struct in6_pktinfo),
		    IS2292(inp, IPV6_2292PKTINFO, IPV6_PKTINFO),
		    IPPROTO_IPV6, mp);
		if (*mp == NULL) {
			return NULL;
		}
	}

	if ((inp->inp_flags & IN6P_HOPLIMIT) != 0) {
		int hlim = ip6->ip6_hlim & 0xff;

		mp = sbcreatecontrol_mbuf((caddr_t)&hlim, sizeof(int),
		    IS2292(inp, IPV6_2292HOPLIMIT, IPV6_HOPLIMIT),
		    IPPROTO_IPV6, mp);
		if (*mp == NULL) {
			return NULL;
		}
	}

	if (v4only != NULL) {
		*v4only = 0;
	}
	return mp;
}

int
ip6_savecontrol(struct inpcb *in6p, struct mbuf *m, struct mbuf **mp)
{
	struct mbuf **np;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	int v4only = 0;

	*mp = NULL;
	np = ip6_savecontrol_v4(in6p, m, mp, &v4only);
	if (np == NULL) {
		goto no_mbufs;
	}

	mp = np;
	if (v4only) {
		return 0;
	}

	if ((in6p->inp_flags & IN6P_TCLASS) != 0) {
		u_int32_t flowinfo;
		int tclass;

		flowinfo = (u_int32_t)ntohl(ip6->ip6_flow & IPV6_FLOWINFO_MASK);
		flowinfo >>= 20;

		tclass = flowinfo & 0xff;
		mp = sbcreatecontrol_mbuf((caddr_t)&tclass, sizeof(tclass),
		    IPV6_TCLASS, IPPROTO_IPV6, mp);
		if (*mp == NULL) {
			goto no_mbufs;
		}
	}

	/*
	 * IPV6_HOPOPTS socket option.  Recall that we required super-user
	 * privilege for the option (see ip6_ctloutput), but it might be too
	 * strict, since there might be some hop-by-hop options which can be
	 * returned to normal user.
	 * See also RFC 2292 section 6 (or RFC 3542 section 8).
	 */
	if ((in6p->inp_flags & IN6P_HOPOPTS) != 0) {
		/*
		 * Check if a hop-by-hop options header is contatined in the
		 * received packet, and if so, store the options as ancillary
		 * data. Note that a hop-by-hop options header must be
		 * just after the IPv6 header, which is assured through the
		 * IPv6 input processing.
		 */
		ip6 = mtod(m, struct ip6_hdr *);
		if (ip6->ip6_nxt == IPPROTO_HOPOPTS) {
			struct ip6_hbh *hbh;
			int hbhlen = 0;
			hbh = (struct ip6_hbh *)(ip6 + 1);
			hbhlen = (hbh->ip6h_len + 1) << 3;

			/*
			 * XXX: We copy the whole header even if a
			 * jumbo payload option is included, the option which
			 * is to be removed before returning according to
			 * RFC2292.
			 * Note: this constraint is removed in RFC3542
			 */
			mp = sbcreatecontrol_mbuf((caddr_t)hbh, hbhlen,
			    IS2292(in6p, IPV6_2292HOPOPTS, IPV6_HOPOPTS),
			    IPPROTO_IPV6, mp);

			if (*mp == NULL) {
				goto no_mbufs;
			}
		}
	}

	if ((in6p->inp_flags & (IN6P_RTHDR | IN6P_DSTOPTS)) != 0) {
		int nxt = ip6->ip6_nxt, off = sizeof(struct ip6_hdr);

		/*
		 * Search for destination options headers or routing
		 * header(s) through the header chain, and stores each
		 * header as ancillary data.
		 * Note that the order of the headers remains in
		 * the chain of ancillary data.
		 */
		while (1) {     /* is explicit loop prevention necessary? */
			struct ip6_ext *ip6e = NULL;
			int elen;

			/*
			 * if it is not an extension header, don't try to
			 * pull it from the chain.
			 */
			switch (nxt) {
			case IPPROTO_DSTOPTS:
			case IPPROTO_ROUTING:
			case IPPROTO_HOPOPTS:
			case IPPROTO_AH: /* is it possible? */
				break;
			default:
				goto loopend;
			}

			if (off + sizeof(*ip6e) > m->m_len) {
				goto loopend;
			}
			ip6e = (struct ip6_ext *)(mtod(m, caddr_t) + off);
			if (nxt == IPPROTO_AH) {
				elen = (ip6e->ip6e_len + 2) << 2;
			} else {
				elen = (ip6e->ip6e_len + 1) << 3;
			}
			if (off + elen > m->m_len) {
				goto loopend;
			}

			switch (nxt) {
			case IPPROTO_DSTOPTS:
				if (!(in6p->inp_flags & IN6P_DSTOPTS)) {
					break;
				}

				mp = sbcreatecontrol_mbuf((caddr_t)ip6e, elen,
				    IS2292(in6p, IPV6_2292DSTOPTS,
				    IPV6_DSTOPTS), IPPROTO_IPV6, mp);
				if (*mp == NULL) {
					goto no_mbufs;
				}
				break;
			case IPPROTO_ROUTING:
				if (!(in6p->inp_flags & IN6P_RTHDR)) {
					break;
				}

				mp = sbcreatecontrol_mbuf((caddr_t)ip6e, elen,
				    IS2292(in6p, IPV6_2292RTHDR, IPV6_RTHDR),
				    IPPROTO_IPV6, mp);
				if (*mp == NULL) {
					goto no_mbufs;
				}
				break;
			case IPPROTO_HOPOPTS:
			case IPPROTO_AH: /* is it possible? */
				break;

			default:
				/*
				 * other cases have been filtered in the above.
				 * none will visit this case.  here we supply
				 * the code just in case (nxt overwritten or
				 * other cases).
				 */
				goto loopend;
			}

			/* proceed with the next header. */
			off += elen;
			nxt = ip6e->ip6e_nxt;
			ip6e = NULL;
		}
loopend:
		;
	}
	return 0;
no_mbufs:
	ip6stat.ip6s_pktdropcntrl++;
	/* XXX increment a stat to show the failure */
	return ENOBUFS;
}
#undef IS2292

void
ip6_notify_pmtu(struct inpcb *in6p, struct sockaddr_in6 *dst, u_int32_t *mtu)
{
	struct socket *__single so;
	mbuf_ref_t m_mtu;
	struct ip6_mtuinfo mtuctl;

	so =  in6p->inp_socket;

	if ((in6p->inp_flags & IN6P_MTU) == 0) {
		return;
	}

	if (mtu == NULL) {
		return;
	}

	if (IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr) && SOCK_CHECK_PROTO(so, IPPROTO_TCP)) {
		return;
	}

	if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr) &&
	    !in6_are_addr_equal_scoped(&in6p->in6p_faddr, &dst->sin6_addr, in6p->inp_fifscope, dst->sin6_scope_id)) {
		return;
	}

	bzero(&mtuctl, sizeof(mtuctl));         /* zero-clear for safety */
	mtuctl.ip6m_mtu = *mtu;
	mtuctl.ip6m_addr = *dst;
	if (!in6_embedded_scope) {
		mtuctl.ip6m_addr.sin6_scope_id = dst->sin6_scope_id;
	}
	if (sa6_recoverscope(&mtuctl.ip6m_addr, TRUE)) {
		return;
	}

	if ((m_mtu = sbcreatecontrol((caddr_t)&mtuctl, sizeof(mtuctl),
	    IPV6_PATHMTU, IPPROTO_IPV6)) == NULL) {
		return;
	}

	if (sbappendaddr(&so->so_rcv, SA(dst), NULL, m_mtu, NULL) == 0) {
		return;
	}
	sorwakeup(so);
}

/*
 * Get pointer to the previous header followed by the header
 * currently processed.
 * XXX: This function supposes that
 *	M includes all headers,
 *	the next header field and the header length field of each header
 *	are valid, and
 *	the sum of each header length equals to OFF.
 * Because of these assumptions, this function must be called very
 * carefully. Moreover, it will not be used in the near future when
 * we develop `neater' mechanism to process extension headers.
 */
char *
ip6_get_prevhdr(struct mbuf *m, int off)
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);

	if (off == sizeof(struct ip6_hdr)) {
		return (char *)&ip6->ip6_nxt;
	} else {
		int len, nxt;
		struct ip6_ext *ip6e = NULL;

		nxt = ip6->ip6_nxt;
		len = sizeof(struct ip6_hdr);
		while (len < off) {
			ip6e = (struct ip6_ext *)(mtod(m, caddr_t) + len);

			switch (nxt) {
			case IPPROTO_FRAGMENT:
				len += sizeof(struct ip6_frag);
				break;
			case IPPROTO_AH:
				len += (ip6e->ip6e_len + 2) << 2;
				break;
			default:
				len += (ip6e->ip6e_len + 1) << 3;
				break;
			}
			nxt = ip6e->ip6e_nxt;
		}
		if (ip6e) {
			return (char *)&ip6e->ip6e_nxt;
		} else {
			return NULL;
		}
	}
}

/*
 * get next header offset.  m will be retained.
 */
int
ip6_nexthdr(struct mbuf *m, int off, int proto, int *nxtp)
{
	struct ip6_hdr ip6;
	struct ip6_ext ip6e;
	struct ip6_frag fh;

	/* just in case */
	VERIFY(m != NULL);
	if ((m->m_flags & M_PKTHDR) == 0 || m->m_pkthdr.len < off) {
		return -1;
	}

	switch (proto) {
	case IPPROTO_IPV6:
		if (m->m_pkthdr.len < off + sizeof(ip6)) {
			return -1;
		}
		m_copydata(m, off, sizeof(ip6), (caddr_t)&ip6);
		if (nxtp) {
			*nxtp = ip6.ip6_nxt;
		}
		off += sizeof(ip6);
		return off;

	case IPPROTO_FRAGMENT:
		/*
		 * terminate parsing if it is not the first fragment,
		 * it does not make sense to parse through it.
		 */
		if (m->m_pkthdr.len < off + sizeof(fh)) {
			return -1;
		}
		m_copydata(m, off, sizeof(fh), (caddr_t)&fh);
		/* IP6F_OFF_MASK = 0xfff8(BigEndian), 0xf8ff(LittleEndian) */
		if (fh.ip6f_offlg & IP6F_OFF_MASK) {
			return -1;
		}
		if (nxtp) {
			*nxtp = fh.ip6f_nxt;
		}
		off += sizeof(struct ip6_frag);
		return off;

	case IPPROTO_AH:
		if (m->m_pkthdr.len < off + sizeof(ip6e)) {
			return -1;
		}
		m_copydata(m, off, sizeof(ip6e), (caddr_t)&ip6e);
		if (nxtp) {
			*nxtp = ip6e.ip6e_nxt;
		}
		off += (ip6e.ip6e_len + 2) << 2;
		return off;

	case IPPROTO_HOPOPTS:
	case IPPROTO_ROUTING:
	case IPPROTO_DSTOPTS:
		if (m->m_pkthdr.len < off + sizeof(ip6e)) {
			return -1;
		}
		m_copydata(m, off, sizeof(ip6e), (caddr_t)&ip6e);
		if (nxtp) {
			*nxtp = ip6e.ip6e_nxt;
		}
		off += (ip6e.ip6e_len + 1) << 3;
		return off;

	case IPPROTO_NONE:
	case IPPROTO_ESP:
	case IPPROTO_IPCOMP:
		/* give up */
		return -1;

	default:
		return -1;
	}
}

/*
 * get offset for the last header in the chain.  m will be kept untainted.
 */
int
ip6_lasthdr(struct mbuf *m, int off, int proto, int *nxtp)
{
	int newoff;
	int nxt;

	if (!nxtp) {
		nxt = -1;
		nxtp = &nxt;
	}
	while (1) {
		newoff = ip6_nexthdr(m, off, proto, nxtp);
		if (newoff < 0) {
			return off;
		} else if (newoff < off) {
			return -1;    /* invalid */
		} else if (newoff == off) {
			return newoff;
		}

		off = newoff;
		proto = *nxtp;
	}
}

boolean_t
ip6_pkt_has_ulp(struct mbuf *m)
{
	int off = 0, nxt = IPPROTO_NONE;

	off = ip6_lasthdr(m, 0, IPPROTO_IPV6, &nxt);
	if (off < 0 || m->m_pkthdr.len < off) {
		return FALSE;
	}

	switch (nxt) {
	case IPPROTO_TCP:
		if (off + sizeof(struct tcphdr) > m->m_pkthdr.len) {
			return FALSE;
		}
		break;
	case IPPROTO_UDP:
		if (off + sizeof(struct udphdr) > m->m_pkthdr.len) {
			return FALSE;
		}
		break;
	case IPPROTO_ICMPV6:
		if (off + sizeof(uint32_t) > m->m_pkthdr.len) {
			return FALSE;
		}
		break;
	case IPPROTO_NONE:
		return TRUE;
	case IPPROTO_ESP:
		return TRUE;
	case IPPROTO_IPCOMP:
		return TRUE;
	default:
		return FALSE;
	}
	return TRUE;
}

struct ip6aux *
ip6_addaux(struct mbuf *m)
{
	struct m_tag *__single tag;

	/* Check if one is already allocated */
	tag = m_tag_locate(m, KERNEL_MODULE_TAG_ID,
	    KERNEL_TAG_TYPE_INET6);
	if (tag == NULL) {
		/* Allocate a tag */
		tag = m_tag_create(KERNEL_MODULE_TAG_ID, KERNEL_TAG_TYPE_INET6,
		    sizeof(struct ip6aux), M_DONTWAIT, m);

		/* Attach it to the mbuf */
		if (tag) {
			m_tag_prepend(m, tag);
		}
	}

	return tag ? (struct ip6aux *)(tag->m_tag_data) : NULL;
}

struct ip6aux *
ip6_findaux(struct mbuf *m)
{
	struct m_tag *__single tag;

	tag = m_tag_locate(m, KERNEL_MODULE_TAG_ID,
	    KERNEL_TAG_TYPE_INET6);

	return tag != NULL ? (struct ip6aux *)(tag->m_tag_data) : NULL;
}

void
ip6_delaux(struct mbuf *m)
{
	struct m_tag *__single tag;

	tag = m_tag_locate(m, KERNEL_MODULE_TAG_ID,
	    KERNEL_TAG_TYPE_INET6);
	if (tag != NULL) {
		m_tag_delete(m, tag);
	}
}

struct inet6_tag_container {
	struct m_tag    inet6_m_tag;
	struct ip6aux   inet6_ip6a;
};

struct m_tag *
m_tag_kalloc_inet6(u_int32_t id, u_int16_t type, uint16_t len, int wait)
{
	struct inet6_tag_container *tag_container;
	struct m_tag *tag = NULL;

	assert3u(id, ==, KERNEL_MODULE_TAG_ID);
	assert3u(type, ==, KERNEL_TAG_TYPE_INET6);
	assert3u(len, ==, sizeof(struct ip6aux));

	if (len != sizeof(struct ip6aux)) {
		return NULL;
	}

	tag_container = kalloc_type(struct inet6_tag_container, wait | M_ZERO);
	if (tag_container != NULL) {
		tag =  &tag_container->inet6_m_tag;

		assert3p(tag, ==, tag_container);

		M_TAG_INIT(tag, id, type, len, &tag_container->inet6_ip6a, NULL);
	}

	return tag;
}

void
m_tag_kfree_inet6(struct m_tag *tag)
{
	struct inet6_tag_container *__single tag_container = (struct inet6_tag_container *)tag;

	assert3u(tag->m_tag_len, ==, sizeof(struct ip6aux));

	kfree_type(struct inet6_tag_container, tag_container);
}

void
ip6_register_m_tag(void)
{
	int error;

	error = m_register_internal_tag_type(KERNEL_TAG_TYPE_INET6, sizeof(struct ip6aux),
	    m_tag_kalloc_inet6, m_tag_kfree_inet6);

	assert3u(error, ==, 0);
}

/*
 * Drain callback
 */
void
ip6_drain(void)
{
	frag6_drain();          /* fragments */
	in6_rtqdrain();         /* protocol cloned routes */
	nd6_drain(NULL);        /* cloned routes: ND6 */
}

/*
 * System control for IP6
 */

u_char  inet6ctlerrmap[PRC_NCMDS] = {
	0, 0, 0, 0,
	0, EMSGSIZE, EHOSTDOWN, EHOSTUNREACH,
	EHOSTUNREACH, EHOSTUNREACH, ECONNREFUSED, ECONNREFUSED,
	EMSGSIZE, EHOSTUNREACH, 0, 0,
	0, 0, 0, 0,
	ENOPROTOOPT, ECONNREFUSED
};

static int
sysctl_reset_ip6_input_stats SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)
	int error, i;

	i = ip6_input_measure;
	error = sysctl_handle_int(oidp, &i, 0, req);
	if (error || req->newptr == USER_ADDR_NULL) {
		goto done;
	}
	/* impose bounds */
	if (i < 0 || i > 1) {
		error = EINVAL;
		goto done;
	}
	if (ip6_input_measure != i && i == 1) {
		net_perf_initialize(&net_perf, ip6_input_measure_bins);
	}
	ip6_input_measure = i;
done:
	return error;
}

static int
sysctl_ip6_input_measure_bins SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)
	int error;
	uint64_t i;

	i = ip6_input_measure_bins;
	error = sysctl_handle_quad(oidp, &i, 0, req);
	if (error || req->newptr == USER_ADDR_NULL) {
		goto done;
	}
	/* validate data */
	if (!net_perf_validate_bins(i)) {
		error = EINVAL;
		goto done;
	}
	ip6_input_measure_bins = i;
done:
	return error;
}

static int
sysctl_ip6_input_getperf SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	if (req->oldptr == USER_ADDR_NULL) {
		req->oldlen = (size_t)sizeof(struct net_perf);
	}

	return SYSCTL_OUT(req, &net_perf, MIN(sizeof(net_perf), req->oldlen));
}


/*
 * Initialize IPv6 source address hash table.
 */
static void
in6_ifaddrhashtbl_init(void)
{
	int i, k, p;
	uint32_t nhash = 0;
	uint32_t hash_size;

	if (in6_ifaddrhashtbl != NULL) {
		return;
	}

	PE_parse_boot_argn("ina6ddr_nhash", &nhash,
	    sizeof(in6addr_nhash));
	if (nhash == 0) {
		nhash = IN6ADDR_NHASH;
	}

	hash_size = nhash * sizeof(*in6_ifaddrhashtbl);

	in6_ifaddrhashtbl = zalloc_permanent(
		hash_size,
		ZALIGN_PTR);
	in6addr_nhash = nhash;

	/*
	 * Generate the next largest prime greater than in6addr_nhash.
	 */
	k = (in6addr_nhash % 2 == 0) ? in6addr_nhash + 1 : in6addr_nhash + 2;
	for (;;) {
		p = 1;
		for (i = 3; i * i <= k; i += 2) {
			if (k % i == 0) {
				p = 0;
			}
		}
		if (p == 1) {
			break;
		}
		k += 2;
	}
	in6addr_hashp = k;
}

static int
sysctl_ip6_checkinterface SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)
	int error, i;

	i = ip6_checkinterface;
	error = sysctl_handle_int(oidp, &i, 0, req);
	if (error || req->newptr == USER_ADDR_NULL) {
		return error;
	}

	switch (i) {
	case IP6_CHECKINTERFACE_WEAK_ES:
	case IP6_CHECKINTERFACE_HYBRID_ES:
	case IP6_CHECKINTERFACE_STRONG_ES:
		if (ip6_checkinterface != i) {
			ip6_checkinterface = i;
			os_log(OS_LOG_DEFAULT, "%s: ip6_checkinterface is now %d\n",
			    __func__, ip6_checkinterface);
		}
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}
