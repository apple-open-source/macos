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

/*	$FreeBSD: src/sys/netinet6/frag6.c,v 1.2.2.5 2001/07/03 11:01:50 ume Exp $	*/
/*	$KAME: frag6.c,v 1.31 2001/05/17 13:45:34 jinmei Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mcache.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <kern/queue.h>
#include <kern/locks.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>

#include <net/net_osdep.h>
#include <dev/random/randomdev.h>

/*
 * Define it to get a correct behavior on per-interface statistics.
 */
#define IN6_IFSTAT_STRICT
struct  ip6asfrag {
	struct ip6asfrag *ip6af_down;
	struct ip6asfrag *ip6af_up;
	struct mbuf     *ip6af_m;
	int             ip6af_offset;   /* offset in ip6af_m to next header */
	int             ip6af_frglen;   /* fragmentable part length */
	int             ip6af_off;      /* fragment offset */
	u_int16_t       ip6af_mff;      /* more fragment bit in frag off */
};

#define IP6_REASS_MBUF(ip6af) ((ip6af)->ip6af_m)

MBUFQ_HEAD(fq6_head);

static void frag6_save_context(struct mbuf *, uintptr_t);
static void frag6_scrub_context(struct mbuf *);
static int frag6_restore_context(struct mbuf *);

static void frag6_icmp6_paramprob_error(struct fq6_head *);
static void frag6_icmp6_timeex_error(struct fq6_head *);

static void frag6_enq(struct ip6asfrag *, struct ip6asfrag *);
static void frag6_deq(struct ip6asfrag *);
static void frag6_insque(struct ip6q *, struct ip6q *);
static void frag6_remque(struct ip6q *);
static void frag6_purgef(struct ip6q *, struct fq6_head *, struct fq6_head *);
static void frag6_freef(struct ip6q *, struct fq6_head *, struct fq6_head *);

static int frag6_timeout_run;           /* frag6 timer is scheduled to run */
static void frag6_timeout(void *);
static void frag6_sched_timeout(void);

static struct ip6q *ip6q_alloc(void);
static void ip6q_free(struct ip6q *);
static void ip6q_updateparams(void);
static struct ip6asfrag *ip6af_alloc(void);
static void ip6af_free(struct ip6asfrag *);

static LCK_GRP_DECLARE(ip6qlock_grp, "ip6qlock");
static LCK_MTX_DECLARE(ip6qlock, &ip6qlock_grp);

/* IPv6 fragment reassembly queues (protected by ip6qlock) */
static struct ip6q ip6q;                /* ip6 reassembly queues */
static int ip6_maxfragpackets;          /* max packets in reass queues */
static u_int32_t frag6_nfragpackets;    /* # of packets in reass queues */
static int ip6_maxfrags;                /* max fragments in reass queues */
static u_int32_t frag6_nfrags;          /* # of fragments in reass queues */
static u_int32_t ip6q_limit;            /* ip6q allocation limit */
static u_int32_t ip6q_count;            /* current # of allocated ip6q's */
static u_int32_t ip6af_limit;           /* ip6asfrag allocation limit */
static u_int32_t ip6af_count;           /* current # of allocated ip6asfrag's */

static int sysctl_maxfragpackets SYSCTL_HANDLER_ARGS;
static int sysctl_maxfrags SYSCTL_HANDLER_ARGS;

SYSCTL_DECL(_net_inet6_ip6);

SYSCTL_PROC(_net_inet6_ip6, IPV6CTL_MAXFRAGPACKETS, maxfragpackets,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED, &ip6_maxfragpackets, 0,
    sysctl_maxfragpackets, "I",
    "Maximum number of IPv6 fragment reassembly queue entries");

SYSCTL_UINT(_net_inet6_ip6, OID_AUTO, fragpackets,
    CTLFLAG_RD | CTLFLAG_LOCKED, &frag6_nfragpackets, 0,
    "Current number of IPv6 fragment reassembly queue entries");

SYSCTL_PROC(_net_inet6_ip6, IPV6CTL_MAXFRAGS, maxfrags,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED, &ip6_maxfrags, 0,
    sysctl_maxfrags, "I", "Maximum number of IPv6 fragments allowed");

/*
 * Initialise reassembly queue and fragment identifier.
 */
void
frag6_init(void)
{
	lck_mtx_lock(&ip6qlock);
	/* Initialize IPv6 reassembly queue. */
	ip6q.ip6q_next = ip6q.ip6q_prev = &ip6q;

	/* same limits as IPv4 */
	ip6_maxfragpackets = nmbclusters / 32;
	ip6_maxfrags = ip6_maxfragpackets * 2;
	ip6q_updateparams();
	lck_mtx_unlock(&ip6qlock);
}

static void
frag6_save_context(struct mbuf *m, uintptr_t val)
{
	m->m_pkthdr.pkt_hdr = __unsafe_forge_single(void *, val);
}

static void
frag6_scrub_context(struct mbuf *m)
{
	m->m_pkthdr.pkt_hdr = NULL;
}

static int
frag6_restore_context(struct mbuf *m)
{
	return (int)m->m_pkthdr.pkt_hdr;
}

/*
 * Send any deferred ICMP param problem error messages; caller must not be
 * holding ip6qlock and is expected to have saved the per-packet parameter
 * value via frag6_save_context().
 */
static void
frag6_icmp6_paramprob_error(struct fq6_head *diq6)
{
	LCK_MTX_ASSERT(&ip6qlock, LCK_MTX_ASSERT_NOTOWNED);

	if (!MBUFQ_EMPTY(diq6)) {
		mbuf_ref_t merr, merr_tmp;
		int param;
		MBUFQ_FOREACH_SAFE(merr, diq6, merr_tmp) {
			MBUFQ_REMOVE(diq6, merr);
			MBUFQ_NEXT(merr) = NULL;
			param = frag6_restore_context(merr);
			frag6_scrub_context(merr);
			icmp6_error(merr, ICMP6_PARAM_PROB,
			    ICMP6_PARAMPROB_HEADER, param);
		}
	}
}

/*
 * Send any deferred ICMP time exceeded error messages;
 * caller must not be holding ip6qlock.
 */
static void
frag6_icmp6_timeex_error(struct fq6_head *diq6)
{
	LCK_MTX_ASSERT(&ip6qlock, LCK_MTX_ASSERT_NOTOWNED);

	if (!MBUFQ_EMPTY(diq6)) {
		mbuf_ref_t m, m_tmp;
		MBUFQ_FOREACH_SAFE(m, diq6, m_tmp) {
			MBUFQ_REMOVE(diq6, m);
			MBUFQ_NEXT(m) = NULL;
			icmp6_error_flag(m, ICMP6_TIME_EXCEEDED,
			    ICMP6_TIME_EXCEED_REASSEMBLY, 0, 0);
		}
	}
}

/*
 * In RFC2460, fragment and reassembly rule do not agree with each other,
 * in terms of next header field handling in fragment header.
 * While the sender will use the same value for all of the fragmented packets,
 * receiver is suggested not to check the consistency.
 *
 * fragment rule (p20):
 *	(2) A Fragment header containing:
 *	The Next Header value that identifies the first header of
 *	the Fragmentable Part of the original packet.
 *		-> next header field is same for all fragments
 *
 * reassembly rule (p21):
 *	The Next Header field of the last header of the Unfragmentable
 *	Part is obtained from the Next Header field of the first
 *	fragment's Fragment header.
 *		-> should grab it from the first fragment only
 *
 * The following note also contradicts with fragment rule - noone is going to
 * send different fragment with different next header field.
 *
 * additional note (p22):
 *	The Next Header values in the Fragment headers of different
 *	fragments of the same original packet may differ.  Only the value
 *	from the Offset zero fragment packet is used for reassembly.
 *		-> should grab it from the first fragment only
 *
 * There is no explicit reason given in the RFC.  Historical reason maybe?
 */
/*
 * Fragment input
 */
int
frag6_input(struct mbuf **mp, int *offp, int proto)
{
#pragma unused(proto)
	mbuf_ref_t m = *mp, t = NULL;
	struct ip6_hdr *ip6 = NULL;
	struct ip6_frag *__single ip6f = NULL;
	struct ip6q *__single q6 = NULL;
	struct ip6asfrag *__single af6 = NULL, *__single ip6af = NULL, *__single af6dwn = NULL;
	int offset = *offp, i = 0, next = 0;
	u_int8_t nxt = 0;
	int first_frag = 0;
	int fragoff = 0, frgpartlen = 0;        /* must be larger than u_int16_t */
	ifnet_ref_t dstifp = NULL;
	u_int8_t ecn = 0, ecn0 = 0;
	uint32_t csum = 0, csum_flags = 0;
	struct fq6_head diq6 = {};
	int locked = 0;
	boolean_t drop_fragq = FALSE;
	int local_ip6q_unfrglen;
	u_int8_t local_ip6q_nxt;

	VERIFY(m->m_flags & M_PKTHDR);

	MBUFQ_INIT(&diq6);      /* for deferred ICMP param problem errors */

	/* Expect 32-bit aligned data pointer on strict-align platforms */
	MBUF_STRICT_DATA_ALIGNMENT_CHECK_32(m);

	IP6_EXTHDR_CHECK(m, offset, sizeof(struct ip6_frag), goto done);
	ip6 = mtod(m, struct ip6_hdr *);
	ip6f = (struct ip6_frag *)((caddr_t)ip6 + offset);

#ifdef IN6_IFSTAT_STRICT
	/* find the destination interface of the packet. */
	if (m->m_pkthdr.pkt_flags & PKTF_IFAINFO) {
		uint32_t idx;

		if (ip6_getdstifaddr_info(m, &idx, NULL) == 0) {
			if (idx > 0 && idx <= if_index) {
				ifnet_head_lock_shared();
				dstifp = ifindex2ifnet[idx];
				ifnet_head_done();
			}
		}
	}
#endif /* IN6_IFSTAT_STRICT */

	/* we are violating the spec, this may not be the dst interface */
	if (dstifp == NULL) {
		dstifp = m->m_pkthdr.rcvif;
	}

	/* jumbo payload can't contain a fragment header */
	if (ip6->ip6_plen == 0) {
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER, offset);
		in6_ifstat_inc(dstifp, ifs6_reass_fail);
		m = NULL;
		goto done;
	}

	/*
	 * check whether fragment packet's fragment length is
	 * multiple of 8 octets.
	 * sizeof(struct ip6_frag) == 8
	 * sizeof(struct ip6_hdr) = 40
	 */
	if ((ip6f->ip6f_offlg & IP6F_MORE_FRAG) &&
	    (((ntohs(ip6->ip6_plen) - offset) & 0x7) != 0)) {
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
		    offsetof(struct ip6_hdr, ip6_plen));
		in6_ifstat_inc(dstifp, ifs6_reass_fail);
		m = NULL;
		goto done;
	}

	/* If ip6_maxfragpackets or ip6_maxfrags is 0, never accept fragments */
	if (ip6_maxfragpackets == 0 || ip6_maxfrags == 0) {
		ip6stat.ip6s_fragments++;
		ip6stat.ip6s_fragdropped++;
		in6_ifstat_inc(dstifp, ifs6_reass_fail);
		m_freem(m);
		m = NULL;
		goto done;
	}

	/* offset now points to data portion */
	offset += sizeof(struct ip6_frag);

	/*
	 * RFC 6946: Handle "atomic" fragments (offset and m bit set to 0)
	 * upfront, unrelated to any reassembly.  Just skip the fragment header.
	 */
	if ((ip6f->ip6f_offlg & ~IP6F_RESERVED_MASK) == 0) {
		/*
		 * Mark packet as reassembled.
		 * In ICMPv6 processing, we drop certain
		 * NDP messages that are not expected to
		 * have fragment header based on recommendations
		 * against security vulnerability as described in
		 * RFC 6980.
		 * Treat atomic fragments as re-assembled packets as well.
		 */
		m->m_pkthdr.pkt_flags |= PKTF_REASSEMBLED;
		ip6stat.ip6s_atmfrag_rcvd++;
		in6_ifstat_inc(dstifp, ifs6_atmfrag_rcvd);
		*mp = m;
		*offp = offset;
		return ip6f->ip6f_nxt;
	}

	/*
	 * Leverage partial checksum offload for simple UDP/IP fragments,
	 * as that is the most common case.
	 *
	 * Perform 1's complement adjustment of octets that got included/
	 * excluded in the hardware-calculated checksum value.  Also take
	 * care of any trailing bytes and subtract out their partial sum.
	 */
	if (ip6f->ip6f_nxt == IPPROTO_UDP &&
	    offset == (sizeof(*ip6) + sizeof(*ip6f)) &&
	    (m->m_pkthdr.csum_flags &
	    (CSUM_DATA_VALID | CSUM_PARTIAL | CSUM_PSEUDO_HDR)) ==
	    (CSUM_DATA_VALID | CSUM_PARTIAL)) {
		uint32_t start = m->m_pkthdr.csum_rx_start;
		uint32_t ip_len = (sizeof(*ip6) + ntohs(ip6->ip6_plen));
		int32_t trailer = (m_pktlen(m) - ip_len);
		uint32_t swbytes = (uint32_t)trailer;

		csum = m->m_pkthdr.csum_rx_val;

		ASSERT(trailer >= 0);
		if (start != offset || trailer != 0) {
			uint16_t s = 0, d = 0;

			if (IN6_IS_SCOPE_EMBED(&ip6->ip6_src)) {
				s = ip6->ip6_src.s6_addr16[1];
				ip6->ip6_src.s6_addr16[1] = 0;
			}
			if (IN6_IS_SCOPE_EMBED(&ip6->ip6_dst)) {
				d = ip6->ip6_dst.s6_addr16[1];
				ip6->ip6_dst.s6_addr16[1] = 0;
			}

			/* callee folds in sum */
			csum = m_adj_sum16(m, start, offset,
			    (ip_len - offset), csum);
			if (offset > start) {
				swbytes += (offset - start);
			} else {
				swbytes += (start - offset);
			}

			if (IN6_IS_SCOPE_EMBED(&ip6->ip6_src)) {
				ip6->ip6_src.s6_addr16[1] = s;
			}
			if (IN6_IS_SCOPE_EMBED(&ip6->ip6_dst)) {
				ip6->ip6_dst.s6_addr16[1] = d;
			}
		}
		csum_flags = m->m_pkthdr.csum_flags;

		if (swbytes != 0) {
			udp_in6_cksum_stats(swbytes);
		}
		if (trailer != 0) {
			m_adj(m, -trailer);
		}
	} else {
		csum = 0;
		csum_flags = 0;
	}

	/* Invalidate checksum */
	m->m_pkthdr.csum_flags &= ~CSUM_DATA_VALID;

	ip6stat.ip6s_fragments++;
	in6_ifstat_inc(dstifp, ifs6_reass_reqd);

	lck_mtx_lock(&ip6qlock);
	locked = 1;

	for (q6 = ip6q.ip6q_next; q6 != &ip6q; q6 = q6->ip6q_next) {
		if (ip6f->ip6f_ident == q6->ip6q_ident &&
		    in6_are_addr_equal_scoped(&ip6->ip6_src, &q6->ip6q_src, ip6_input_getsrcifscope(m), q6->ip6q_src_ifscope) &&
		    in6_are_addr_equal_scoped(&ip6->ip6_dst, &q6->ip6q_dst, ip6_input_getdstifscope(m), q6->ip6q_dst_ifscope)) {
			break;
		}
	}

	if (q6 == &ip6q) {
		/*
		 * Create a reassembly queue as this is the first fragment to
		 * arrive.
		 * By first frag, we don't mean the one with offset 0, but
		 * any of the fragments of the fragmented packet that has
		 * reached us first.
		 */
		first_frag = 1;

		q6 = ip6q_alloc();
		if (q6 == NULL) {
			goto dropfrag;
		}

		frag6_insque(q6, &ip6q);
		frag6_nfragpackets++;

		/* ip6q_nxt will be filled afterwards, from 1st fragment */
		q6->ip6q_down   = q6->ip6q_up = (struct ip6asfrag *)q6;
#ifdef notyet
		q6->ip6q_nxtp   = (u_char *)nxtp;
#endif
		q6->ip6q_ident  = ip6f->ip6f_ident;
		q6->ip6q_ttl    = IPV6_FRAGTTL;
		q6->ip6q_src    = ip6->ip6_src;
		q6->ip6q_dst    = ip6->ip6_dst;
		q6->ip6q_dst_ifscope = IN6_IS_SCOPE_EMBED(&q6->ip6q_dst) ? ip6_input_getdstifscope(m) : IFSCOPE_NONE;
		q6->ip6q_src_ifscope = IN6_IS_SCOPE_EMBED(&q6->ip6q_src) ? ip6_input_getsrcifscope(m) : IFSCOPE_NONE;
		q6->ip6q_ecn    =
		    (ntohl(ip6->ip6_flow) >> 20) & IPTOS_ECN_MASK;
		q6->ip6q_unfrglen = -1; /* The 1st fragment has not arrived. */

		q6->ip6q_nfrag = 0;
		q6->ip6q_flags = 0;

		/*
		 * If the first fragment has valid checksum offload
		 * info, the rest of fragments are eligible as well.
		 */
		if (csum_flags != 0) {
			q6->ip6q_csum = csum;
			q6->ip6q_csum_flags = csum_flags;
		}
	}

	if (q6->ip6q_flags & IP6QF_DIRTY) {
		goto dropfrag;
	}

	local_ip6q_unfrglen = q6->ip6q_unfrglen;
	local_ip6q_nxt = q6->ip6q_nxt;

	/*
	 * If it's the 1st fragment, record the length of the
	 * unfragmentable part and the next header of the fragment header.
	 * Assume the first fragement to arrive will be correct.
	 * We do not have any duplicate checks here yet so another packet
	 * with fragoff == 0 could come and overwrite the ip6q_unfrglen
	 * and worse, the next header, at any time.
	 */
	fragoff = ntohs(ip6f->ip6f_offlg & IP6F_OFF_MASK);
	if (fragoff == 0 && local_ip6q_unfrglen == -1) {
		local_ip6q_unfrglen = offset - sizeof(struct ip6_hdr) -
		    sizeof(struct ip6_frag);
		local_ip6q_nxt = ip6f->ip6f_nxt;
		/* XXX ECN? */
	}

	/*
	 * Check that the reassembled packet would not exceed 65535 bytes
	 * in size.
	 * If it would exceed, discard the fragment and return an ICMP error.
	 */
	frgpartlen = sizeof(struct ip6_hdr) + ntohs(ip6->ip6_plen) - offset;
	if (local_ip6q_unfrglen >= 0) {
		/* The 1st fragment has already arrived. */
		if (local_ip6q_unfrglen + fragoff + frgpartlen > IPV6_MAXPACKET) {
			lck_mtx_unlock(&ip6qlock);
			locked = 0;
			icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
			    offset - sizeof(struct ip6_frag) +
			    offsetof(struct ip6_frag, ip6f_offlg));
			m = NULL;
			goto done;
		}
	} else if (fragoff + frgpartlen > IPV6_MAXPACKET) {
		lck_mtx_unlock(&ip6qlock);
		locked = 0;
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
		    offset - sizeof(struct ip6_frag) +
		    offsetof(struct ip6_frag, ip6f_offlg));
		m = NULL;
		goto done;
	}
	/*
	 * If it's the first fragment, do the above check for each
	 * fragment already stored in the reassembly queue.
	 */
	if (fragoff == 0) {
		/*
		 * https://tools.ietf.org/html/rfc8200#page-20
		 * If the first fragment does not include all headers through an
		 * Upper-Layer header, then that fragment should be discarded and
		 * an ICMP Parameter Problem, Code 3, message should be sent to
		 * the source of the fragment, with the Pointer field set to zero.
		 */
		if (!ip6_pkt_has_ulp(m)) {
			lck_mtx_unlock(&ip6qlock);
			locked = 0;
			icmp6_error(m, ICMP6_PARAM_PROB,
			    ICMP6_PARAMPROB_FIRSTFRAG_INCOMP_HDR, 0);
			m = NULL;
			goto done;
		}
		for (af6 = q6->ip6q_down; af6 != (struct ip6asfrag *)q6;
		    af6 = af6dwn) {
			af6dwn = af6->ip6af_down;

			if (local_ip6q_unfrglen + af6->ip6af_off + af6->ip6af_frglen >
			    IPV6_MAXPACKET) {
				mbuf_ref_t merr = IP6_REASS_MBUF(af6);
				struct ip6_hdr *__single ip6err;
				int erroff = af6->ip6af_offset;

				/* dequeue the fragment. */
				frag6_deq(af6);
				ip6af_free(af6);

				/* adjust pointer. */
				ip6err = mtod(merr, struct ip6_hdr *);

				/*
				 * Restore source and destination addresses
				 * in the erroneous IPv6 header.
				 */
				ip6err->ip6_src = q6->ip6q_src;
				ip6err->ip6_dst = q6->ip6q_dst;
				ip6_output_setdstifscope(m, q6->ip6q_dst_ifscope, NULL);
				ip6_output_setsrcifscope(m, q6->ip6q_src_ifscope, NULL);
				frag6_save_context(merr,
				    erroff - sizeof(struct ip6_frag) +
				    offsetof(struct ip6_frag, ip6f_offlg));

				MBUFQ_ENQUEUE(&diq6, merr);
			}
		}
	}

	ip6af = ip6af_alloc();
	if (ip6af == NULL) {
		goto dropfrag;
	}

	ip6af->ip6af_mff = ip6f->ip6f_offlg & IP6F_MORE_FRAG;
	ip6af->ip6af_off = fragoff;
	ip6af->ip6af_frglen = frgpartlen;
	ip6af->ip6af_offset = offset;
	IP6_REASS_MBUF(ip6af) = m;

	if (first_frag) {
		af6 = (struct ip6asfrag *)q6;
		goto insert;
	}

	/*
	 * Handle ECN by comparing this segment with the first one;
	 * if CE is set, do not lose CE.
	 * drop if CE and not-ECT are mixed for the same packet.
	 */
	ecn = (ntohl(ip6->ip6_flow) >> 20) & IPTOS_ECN_MASK;
	ecn0 = q6->ip6q_ecn;
	if (ecn == IPTOS_ECN_CE) {
		if (ecn0 == IPTOS_ECN_NOTECT) {
			ip6af_free(ip6af);
			goto dropfrag;
		}
		if (ecn0 != IPTOS_ECN_CE) {
			q6->ip6q_ecn = IPTOS_ECN_CE;
		}
	}
	if (ecn == IPTOS_ECN_NOTECT && ecn0 != IPTOS_ECN_NOTECT) {
		ip6af_free(ip6af);
		goto dropfrag;
	}

	/*
	 * Find a segment which begins after this one does.
	 */
	for (af6 = q6->ip6q_down; af6 != (struct ip6asfrag *)q6;
	    af6 = af6->ip6af_down) {
		if (af6->ip6af_off > ip6af->ip6af_off) {
			break;
		}
	}

	/*
	 * As per RFC 8200 reassembly rules, we MUST drop the entire
	 * chain of fragments for a packet to be assembled, if we receive
	 * any overlapping fragments.
	 * https://tools.ietf.org/html/rfc8200#page-20
	 *
	 * To avoid more conditional code, just reuse frag6_freef and defer
	 * its call to post fragment insertion in the queue.
	 */
	if (af6->ip6af_up != (struct ip6asfrag *)q6) {
		if (af6->ip6af_up->ip6af_off == ip6af->ip6af_off) {
			if (af6->ip6af_up->ip6af_frglen != ip6af->ip6af_frglen) {
				drop_fragq = TRUE;
			} else {
				/*
				 * XXX Ideally we should be comparing the entire
				 * packet here but for now just use off and fraglen
				 * to ignore a duplicate fragment.
				 */
				ip6af_free(ip6af);
				goto dropfrag;
			}
		} else {
			i = af6->ip6af_up->ip6af_off + af6->ip6af_up->ip6af_frglen
			    - ip6af->ip6af_off;
			if (i > 0) {
				drop_fragq = TRUE;
			}
		}
	}

	if (af6 != (struct ip6asfrag *)q6) {
		/*
		 * Given that we break when af6->ip6af_off > ip6af->ip6af_off,
		 * we shouldn't need a check for duplicate fragment here.
		 * For now just assert.
		 */
		VERIFY(af6->ip6af_off != ip6af->ip6af_off);
		i = (ip6af->ip6af_off + ip6af->ip6af_frglen) - af6->ip6af_off;
		if (i > 0) {
			drop_fragq = TRUE;
		}
	}

	/*
	 * If this fragment contains similar checksum offload info
	 * as that of the existing ones, accumulate checksum.  Otherwise,
	 * invalidate checksum offload info for the entire datagram.
	 */
	if (csum_flags != 0 && csum_flags == q6->ip6q_csum_flags) {
		q6->ip6q_csum += csum;
	} else if (q6->ip6q_csum_flags != 0) {
		q6->ip6q_csum_flags = 0;
	}

insert:
	/*
	 * Stick new segment in its place;
	 * check for complete reassembly.
	 * Move to front of packet queue, as we are
	 * the most recently active fragmented packet.
	 */
	frag6_enq(ip6af, af6->ip6af_up);
	frag6_nfrags++;
	q6->ip6q_nfrag++;

	/*
	 * This holds true, when we receive overlapping fragments.
	 * We must silently drop all the fragments we have received
	 * so far.
	 * Also mark q6 as dirty, so as to not add any new fragments to it.
	 * Make sure even q6 marked dirty is kept till timer expires for
	 * reassembly and when that happens, silenty get rid of q6
	 */
	if (drop_fragq) {
		struct fq6_head dfq6 = {0};
		MBUFQ_INIT(&dfq6);      /* for deferred frees */
		q6->ip6q_flags |= IP6QF_DIRTY;
		/* Purge all the fragments but do not free q6 */
		frag6_purgef(q6, &dfq6, NULL);
		af6 = NULL;

		/* free fragments that need to be freed */
		if (!MBUFQ_EMPTY(&dfq6)) {
			MBUFQ_DRAIN(&dfq6);
		}
		VERIFY(MBUFQ_EMPTY(&dfq6));
		/*
		 * Just in case the above logic got anything added
		 * to diq6, drain it.
		 * Please note that these mbufs are not present in the
		 * fragment queue and are added to diq6 for sending
		 * ICMPv6 error.
		 * Given that the current fragment was an overlapping
		 * fragment and the RFC requires us to not send any
		 * ICMPv6 errors while purging the entire queue.
		 * Just empty it out.
		 */
		if (!MBUFQ_EMPTY(&diq6)) {
			MBUFQ_DRAIN(&diq6);
		}
		VERIFY(MBUFQ_EMPTY(&diq6));
		/*
		 * MBUFQ_DRAIN would have drained all the mbufs
		 * in the fragment queue.
		 * This shouldn't be needed as we are returning IPPROTO_DONE
		 * from here but change the passed mbuf pointer to NULL.
		 */
		*mp = NULL;
		lck_mtx_unlock(&ip6qlock);
		return IPPROTO_DONE;
	}

	/*
	 * We're keeping the fragment.
	 */
	q6->ip6q_unfrglen = local_ip6q_unfrglen;
	q6->ip6q_nxt = local_ip6q_nxt;

	next = 0;
	for (af6 = q6->ip6q_down; af6 != (struct ip6asfrag *)q6;
	    af6 = af6->ip6af_down) {
		if (af6->ip6af_off != next) {
			lck_mtx_unlock(&ip6qlock);
			locked = 0;
			m = NULL;
			goto done;
		}
		next += af6->ip6af_frglen;
	}
	if (af6->ip6af_up->ip6af_mff) {
		lck_mtx_unlock(&ip6qlock);
		locked = 0;
		m = NULL;
		goto done;
	}

	/*
	 * Reassembly is complete; concatenate fragments.
	 */
	ip6af = q6->ip6q_down;
	t = m = IP6_REASS_MBUF(ip6af);
	af6 = ip6af->ip6af_down;
	frag6_deq(ip6af);
	while (af6 != (struct ip6asfrag *)q6) {
		af6dwn = af6->ip6af_down;
		frag6_deq(af6);
		while (t->m_next) {
			t = t->m_next;
		}
		t->m_next = IP6_REASS_MBUF(af6);
		m_adj(t->m_next, af6->ip6af_offset);
		ip6af_free(af6);
		af6 = af6dwn;
	}

	/*
	 * Store partial hardware checksum info from the fragment queue;
	 * the receive start offset is set to 40 bytes (see code at the
	 * top of this routine.)
	 */
	if (q6->ip6q_csum_flags != 0) {
		csum = q6->ip6q_csum;

		ADDCARRY(csum);

		m->m_pkthdr.csum_rx_val = (u_int16_t)csum;
		m->m_pkthdr.csum_rx_start = sizeof(struct ip6_hdr);
		m->m_pkthdr.csum_flags = q6->ip6q_csum_flags;
	} else if ((m->m_pkthdr.rcvif->if_flags & IFF_LOOPBACK) ||
	    (m->m_pkthdr.pkt_flags & PKTF_LOOP)) {
		/* loopback checksums are always OK */
		m->m_pkthdr.csum_data = 0xffff;
		m->m_pkthdr.csum_flags = CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
	}

	/* adjust offset to point where the original next header starts */
	offset = ip6af->ip6af_offset - sizeof(struct ip6_frag);
	ip6af_free(ip6af);
	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_plen = htons((uint16_t)(next + offset - sizeof(struct ip6_hdr)));
	ip6->ip6_src = q6->ip6q_src;
	ip6->ip6_dst = q6->ip6q_dst;
	ip6_output_setdstifscope(m, q6->ip6q_dst_ifscope, NULL);
	ip6_output_setsrcifscope(m, q6->ip6q_src_ifscope, NULL);
	if (q6->ip6q_ecn == IPTOS_ECN_CE) {
		ip6->ip6_flow |= htonl(IPTOS_ECN_CE << 20);
	}

	nxt = q6->ip6q_nxt;
#ifdef notyet
	*q6->ip6q_nxtp = (u_char)(nxt & 0xff);
#endif

	/* Delete frag6 header */
	if (m->m_len >= offset + sizeof(struct ip6_frag)) {
		/* This is the only possible case with !PULLDOWN_TEST */
		ovbcopy((caddr_t)ip6, (caddr_t)ip6 + sizeof(struct ip6_frag),
		    offset);
		m->m_data += sizeof(struct ip6_frag);
		m->m_len -= sizeof(struct ip6_frag);
	} else {
		/* this comes with no copy if the boundary is on cluster */
		if ((t = m_split(m, offset, M_DONTWAIT)) == NULL) {
			frag6_remque(q6);
			frag6_nfragpackets--;
			frag6_nfrags -= q6->ip6q_nfrag;
			ip6q_free(q6);
			goto dropfrag;
		}
		m_adj(t, sizeof(struct ip6_frag));
		m_cat(m, t);
	}

	/*
	 * Store NXT to the original.
	 */
	{
		char *prvnxtp = ip6_get_prevhdr(m, offset); /* XXX */
		*prvnxtp = nxt;
	}

	frag6_remque(q6);
	frag6_nfragpackets--;
	frag6_nfrags -= q6->ip6q_nfrag;
	ip6q_free(q6);

	if (m->m_flags & M_PKTHDR) {    /* Isn't it always true? */
		m_fixhdr(m);
		/*
		 * Mark packet as reassembled
		 * In ICMPv6 processing, we drop certain
		 * NDP messages that are not expected to
		 * have fragment header based on recommendations
		 * against security vulnerability as described in
		 * RFC 6980.
		 */
		m->m_pkthdr.pkt_flags |= PKTF_REASSEMBLED;
	}
	ip6stat.ip6s_reassembled++;

	/*
	 * Tell launch routine the next header
	 */
	*mp = m;
	*offp = offset;

	/* arm the purge timer if not already and if there's work to do */
	frag6_sched_timeout();
	lck_mtx_unlock(&ip6qlock);
	in6_ifstat_inc(dstifp, ifs6_reass_ok);
	frag6_icmp6_paramprob_error(&diq6);
	VERIFY(MBUFQ_EMPTY(&diq6));
	return nxt;

done:
	VERIFY(m == NULL);
	*mp = m;
	if (!locked) {
		if (frag6_nfragpackets == 0) {
			frag6_icmp6_paramprob_error(&diq6);
			VERIFY(MBUFQ_EMPTY(&diq6));
			return IPPROTO_DONE;
		}
		lck_mtx_lock(&ip6qlock);
	}
	/* arm the purge timer if not already and if there's work to do */
	frag6_sched_timeout();
	lck_mtx_unlock(&ip6qlock);
	frag6_icmp6_paramprob_error(&diq6);
	VERIFY(MBUFQ_EMPTY(&diq6));
	return IPPROTO_DONE;

dropfrag:
	ip6stat.ip6s_fragdropped++;
	/* arm the purge timer if not already and if there's work to do */
	frag6_sched_timeout();
	lck_mtx_unlock(&ip6qlock);
	in6_ifstat_inc(dstifp, ifs6_reass_fail);
	m_freem(m);
	*mp = NULL;
	frag6_icmp6_paramprob_error(&diq6);
	VERIFY(MBUFQ_EMPTY(&diq6));
	return IPPROTO_DONE;
}

/*
 * This routine removes the enqueued frames from the passed fragment
 * header and enqueues those to dfq6 which is an out-arg for the dequeued
 * fragments.
 * If the caller also provides diq6, this routine also enqueues the 0 offset
 * fragment to that list as it potentially gets used by the caller
 * to prepare the relevant ICMPv6 error message (time exceeded or
 * param problem).
 * It leaves the fragment header object (q6) intact.
 */
static void
frag6_purgef(struct ip6q *q6, struct fq6_head *dfq6, struct fq6_head *diq6)
{
	struct ip6asfrag *__single af6 = NULL;
	struct ip6asfrag *__single down6 = NULL;

	LCK_MTX_ASSERT(&ip6qlock, LCK_MTX_ASSERT_OWNED);

	for (af6 = q6->ip6q_down; af6 != (struct ip6asfrag *)q6;
	    af6 = down6) {
		mbuf_ref_t m = IP6_REASS_MBUF(af6);

		down6 = af6->ip6af_down;
		frag6_deq(af6);

		/*
		 * If caller wants to generate ICMP time-exceeded,
		 * as indicated by the argument diq6, return it for
		 * the first fragment and add others to the fragment
		 * free queue.
		 */
		if (af6->ip6af_off == 0 && diq6 != NULL) {
			struct ip6_hdr *__single ip6;

			/* adjust pointer */
			ip6 = mtod(m, struct ip6_hdr *);

			/* restore source and destination addresses */
			ip6->ip6_src = q6->ip6q_src;
			ip6->ip6_dst = q6->ip6q_dst;
			ip6_output_setdstifscope(m, q6->ip6q_dst_ifscope, NULL);
			ip6_output_setsrcifscope(m, q6->ip6q_src_ifscope, NULL);
			MBUFQ_ENQUEUE(diq6, m);
		} else {
			MBUFQ_ENQUEUE(dfq6, m);
		}
		ip6af_free(af6);
	}
}

/*
 * This routine removes the enqueued frames from the passed fragment
 * header and enqueues those to dfq6 which is an out-arg for the dequeued
 * fragments.
 * If the caller also provides diq6, this routine also enqueues the 0 offset
 * fragment to that list as it potentially gets used by the caller
 * to prepare the relevant ICMPv6 error message (time exceeded or
 * param problem).
 * It also remove the fragment header object from the queue and frees it.
 */
static void
frag6_freef(struct ip6q *q6, struct fq6_head *dfq6, struct fq6_head *diq6)
{
	frag6_purgef(q6, dfq6, diq6);
	frag6_remque(q6);
	frag6_nfragpackets--;
	frag6_nfrags -= q6->ip6q_nfrag;
	ip6q_free(q6);
}

/*
 * Put an ip fragment on a reassembly chain.
 * Like insque, but pointers in middle of structure.
 */
void
frag6_enq(struct ip6asfrag *af6, struct ip6asfrag *up6)
{
	LCK_MTX_ASSERT(&ip6qlock, LCK_MTX_ASSERT_OWNED);

	af6->ip6af_up = up6;
	af6->ip6af_down = up6->ip6af_down;
	up6->ip6af_down->ip6af_up = af6;
	up6->ip6af_down = af6;
}

/*
 * To frag6_enq as remque is to insque.
 */
void
frag6_deq(struct ip6asfrag *af6)
{
	LCK_MTX_ASSERT(&ip6qlock, LCK_MTX_ASSERT_OWNED);

	af6->ip6af_up->ip6af_down = af6->ip6af_down;
	af6->ip6af_down->ip6af_up = af6->ip6af_up;
}

void
frag6_insque(struct ip6q *new, struct ip6q *old)
{
	LCK_MTX_ASSERT(&ip6qlock, LCK_MTX_ASSERT_OWNED);

	new->ip6q_prev = old;
	new->ip6q_next = old->ip6q_next;
	old->ip6q_next->ip6q_prev = new;
	old->ip6q_next = new;
}

void
frag6_remque(struct ip6q *p6)
{
	LCK_MTX_ASSERT(&ip6qlock, LCK_MTX_ASSERT_OWNED);

	p6->ip6q_prev->ip6q_next = p6->ip6q_next;
	p6->ip6q_next->ip6q_prev = p6->ip6q_prev;
}

/*
 * IPv6 reassembling timer processing;
 * if a timer expires on a reassembly
 * queue, discard it.
 */
static void
frag6_timeout(void *arg)
{
#pragma unused(arg)
	struct fq6_head dfq6, diq6;
	struct fq6_head *__single diq6_tmp = NULL;
	struct ip6q *__single q6;

	MBUFQ_INIT(&dfq6);      /* for deferred frees */
	MBUFQ_INIT(&diq6);      /* for deferred ICMP time exceeded errors */

	/*
	 * Update coarse-grained networking timestamp (in sec.); the idea
	 * is to piggy-back on the timeout callout to update the counter
	 * returnable via net_uptime().
	 */
	net_update_uptime();

	lck_mtx_lock(&ip6qlock);
	q6 = ip6q.ip6q_next;
	if (q6) {
		while (q6 != &ip6q) {
			--q6->ip6q_ttl;
			q6 = q6->ip6q_next;
			if (q6->ip6q_prev->ip6q_ttl == 0) {
				ip6stat.ip6s_fragtimeout++;
				/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
				/*
				 * Avoid sending ICMPv6 Time Exceeded for fragment headers
				 * that are marked dirty.
				 */
				diq6_tmp = (q6->ip6q_prev->ip6q_flags & IP6QF_DIRTY) ?
				    NULL : &diq6;
				frag6_freef(q6->ip6q_prev, &dfq6, diq6_tmp);
			}
		}
	}
	/*
	 * If we are over the maximum number of fragments
	 * (due to the limit being lowered), drain off
	 * enough to get down to the new limit.
	 */
	if (ip6_maxfragpackets >= 0) {
		while (frag6_nfragpackets > (unsigned)ip6_maxfragpackets &&
		    ip6q.ip6q_prev) {
			ip6stat.ip6s_fragoverflow++;
			/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
			/*
			 * Avoid sending ICMPv6 Time Exceeded for fragment headers
			 * that are marked dirty.
			 */
			diq6_tmp = (ip6q.ip6q_prev->ip6q_flags & IP6QF_DIRTY) ?
			    NULL : &diq6;
			frag6_freef(ip6q.ip6q_prev, &dfq6, diq6_tmp);
		}
	}
	/* re-arm the purge timer if there's work to do */
	frag6_timeout_run = 0;
	frag6_sched_timeout();
	lck_mtx_unlock(&ip6qlock);

	/* free fragments that need to be freed */
	if (!MBUFQ_EMPTY(&dfq6)) {
		MBUFQ_DRAIN(&dfq6);
	}

	frag6_icmp6_timeex_error(&diq6);

	VERIFY(MBUFQ_EMPTY(&dfq6));
	VERIFY(MBUFQ_EMPTY(&diq6));
}

static void
frag6_sched_timeout(void)
{
	LCK_MTX_ASSERT(&ip6qlock, LCK_MTX_ASSERT_OWNED);

	if (!frag6_timeout_run && frag6_nfragpackets > 0) {
		frag6_timeout_run = 1;
		timeout(frag6_timeout, NULL, hz);
	}
}

/*
 * Drain off all datagram fragments.
 */
void
frag6_drain(void)
{
	struct fq6_head dfq6, diq6;
	struct fq6_head *__single diq6_tmp = NULL;

	MBUFQ_INIT(&dfq6);      /* for deferred frees */
	MBUFQ_INIT(&diq6);      /* for deferred ICMP time exceeded errors */

	lck_mtx_lock(&ip6qlock);
	while (ip6q.ip6q_next != &ip6q) {
		ip6stat.ip6s_fragdropped++;
		/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
		/*
		 * Avoid sending ICMPv6 Time Exceeded for fragment headers
		 * that are marked dirty.
		 */
		diq6_tmp = (ip6q.ip6q_next->ip6q_flags & IP6QF_DIRTY) ?
		    NULL : &diq6;
		frag6_freef(ip6q.ip6q_next, &dfq6, diq6_tmp);
	}
	lck_mtx_unlock(&ip6qlock);

	/* free fragments that need to be freed */
	if (!MBUFQ_EMPTY(&dfq6)) {
		MBUFQ_DRAIN(&dfq6);
	}

	frag6_icmp6_timeex_error(&diq6);

	VERIFY(MBUFQ_EMPTY(&dfq6));
	VERIFY(MBUFQ_EMPTY(&diq6));
}

static struct ip6q *
ip6q_alloc(void)
{
	struct ip6q *__single q6;

	/*
	 * See comments in ip6q_updateparams().  Keep the count separate
	 * from frag6_nfragpackets since the latter represents the elements
	 * already in the reassembly queues.
	 */
	if (ip6q_limit > 0 && ip6q_count > ip6q_limit) {
		return NULL;
	}

	q6 = kalloc_type(struct ip6q, Z_NOWAIT | Z_ZERO);
	if (q6 != NULL) {
		os_atomic_inc(&ip6q_count, relaxed);
	}
	return q6;
}

static void
ip6q_free(struct ip6q *q6)
{
	kfree_type(struct ip6q, q6);
	os_atomic_dec(&ip6q_count, relaxed);
}

static struct ip6asfrag *
ip6af_alloc(void)
{
	struct ip6asfrag *__single af6;

	/*
	 * See comments in ip6q_updateparams().  Keep the count separate
	 * from frag6_nfrags since the latter represents the elements
	 * already in the reassembly queues.
	 */
	if (ip6af_limit > 0 && ip6af_count > ip6af_limit) {
		return NULL;
	}

	af6 = kalloc_type(struct ip6asfrag, Z_NOWAIT | Z_ZERO);
	if (af6 != NULL) {
		os_atomic_inc(&ip6af_count, relaxed);
	}
	return af6;
}

static void
ip6af_free(struct ip6asfrag *af6)
{
	kfree_type(struct ip6asfrag, af6);
	os_atomic_dec(&ip6af_count, relaxed);
}

static void
ip6q_updateparams(void)
{
	LCK_MTX_ASSERT(&ip6qlock, LCK_MTX_ASSERT_OWNED);
	/*
	 * -1 for unlimited allocation.
	 */
	if (ip6_maxfragpackets < 0) {
		ip6q_limit = 0;
	}
	if (ip6_maxfrags < 0) {
		ip6af_limit = 0;
	}
	/*
	 * Positive number for specific bound.
	 */
	if (ip6_maxfragpackets > 0) {
		ip6q_limit = ip6_maxfragpackets;
	}
	if (ip6_maxfrags > 0) {
		ip6af_limit = ip6_maxfrags;
	}
	/*
	 * Zero specifies no further fragment queue allocation -- set the
	 * bound very low, but rely on implementation elsewhere to actually
	 * prevent allocation and reclaim current queues.
	 */
	if (ip6_maxfragpackets == 0) {
		ip6q_limit = 1;
	}
	if (ip6_maxfrags == 0) {
		ip6af_limit = 1;
	}
	/*
	 * Arm the purge timer if not already and if there's work to do
	 */
	frag6_sched_timeout();
}

static int
sysctl_maxfragpackets SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)
	int error, i;

	lck_mtx_lock(&ip6qlock);
	i = ip6_maxfragpackets;
	error = sysctl_handle_int(oidp, &i, 0, req);
	if (error || req->newptr == USER_ADDR_NULL) {
		goto done;
	}
	/* impose bounds */
	if (i < -1 || i > (nmbclusters / 4)) {
		error = EINVAL;
		goto done;
	}
	ip6_maxfragpackets = i;
	ip6q_updateparams();
done:
	lck_mtx_unlock(&ip6qlock);
	return error;
}

static int
sysctl_maxfrags SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)
	int error, i;

	lck_mtx_lock(&ip6qlock);
	i = ip6_maxfrags;
	error = sysctl_handle_int(oidp, &i, 0, req);
	if (error || req->newptr == USER_ADDR_NULL) {
		goto done;
	}
	/* impose bounds */
	if (i < -1 || i > (nmbclusters / 4)) {
		error = EINVAL;
		goto done;
	}
	ip6_maxfrags = i;
	ip6q_updateparams();    /* see if we need to arm timer */
done:
	lck_mtx_unlock(&ip6qlock);
	return error;
}
