/*
 * Copyright (c) 2011-2021 Apple Inc. All rights reserved.
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

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/mcache.h>
#include <sys/sysctl.h>

#include <dev/random/randomdev.h>
#include <net/droptap.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/net_osdep.h>
#include <net/droptap.h>
#include <net/pktsched/pktsched.h>
#include <net/pktsched/pktsched_fq_codel.h>
#include <net/pktsched/pktsched_netem.h>

#define _IP_VHL
#include <netinet/ip.h>
#include <netinet/ip6.h>

#include <pexpert/pexpert.h>

#if SKYWALK
#include <skywalk/os_skywalk_private.h>
#endif /* SKYWALK */

u_int32_t machclk_freq = 0;
u_int64_t machclk_per_sec = 0;
u_int32_t pktsched_verbose = 0; /* more noise if greater than 1 */

static void init_machclk(void);

SYSCTL_NODE(_net, OID_AUTO, pktsched, CTLFLAG_RW | CTLFLAG_LOCKED, 0, "pktsched");

SYSCTL_UINT(_net_pktsched, OID_AUTO, verbose, CTLFLAG_RW | CTLFLAG_LOCKED,
    &pktsched_verbose, 0, "Packet scheduler verbosity level");

void
pktsched_init(void)
{
	init_machclk();
	if (machclk_freq == 0) {
		panic("%s: no CPU clock available!", __func__);
		/* NOTREACHED */
	}
	pktsched_fq_init();
}

static void
init_machclk(void)
{
	/*
	 * Initialize machclk_freq using the timerbase frequency
	 * value from device specific info.
	 */
	machclk_freq = (uint32_t)gPEClockFrequencyInfo.timebase_frequency_hz;

	clock_interval_to_absolutetime_interval(1, NSEC_PER_SEC,
	    &machclk_per_sec);
}

u_int64_t
pktsched_abs_to_nsecs(u_int64_t abstime)
{
	u_int64_t nsecs;

	absolutetime_to_nanoseconds(abstime, &nsecs);
	return nsecs;
}

u_int64_t
pktsched_nsecs_to_abstime(u_int64_t nsecs)
{
	u_int64_t abstime;

	nanoseconds_to_absolutetime(nsecs, &abstime);
	return abstime;
}

int
pktsched_setup(struct ifclassq *ifq, u_int32_t scheduler, u_int32_t sflags,
    classq_pkt_type_t ptype)
{
	int error = 0;
	u_int32_t rflags;

	IFCQ_LOCK_ASSERT_HELD(ifq);

	VERIFY(machclk_freq != 0);

	/* Nothing to do unless the scheduler type changes */
	if (ifq->ifcq_type == scheduler) {
		return 0;
	}

	/*
	 * Remember the flags that need to be restored upon success, as
	 * they may be cleared when we tear down existing scheduler.
	 */
	rflags = (ifq->ifcq_flags & IFCQF_ENABLED);

	if (ifq->ifcq_type != PKTSCHEDT_NONE) {
		pktsched_teardown(ifq);

		/* Teardown should have succeeded */
		VERIFY(ifq->ifcq_type == PKTSCHEDT_NONE);
		VERIFY(ifq->ifcq_disc == NULL);
	}

	error = fq_if_setup_ifclassq(ifq, sflags, ptype);
	if (error == 0) {
		ifq->ifcq_flags |= rflags;
	}

	return error;
}

void
pktsched_teardown(struct ifclassq *ifq)
{
	IFCQ_LOCK_ASSERT_HELD(ifq);
	if_qflush(ifq->ifcq_ifp, ifq, true);
	VERIFY(IFCQ_IS_EMPTY(ifq));
	ifq->ifcq_flags &= ~IFCQF_ENABLED;
	if (ifq->ifcq_type == PKTSCHEDT_FQ_CODEL) {
		/* Could be PKTSCHEDT_NONE */
		fq_if_teardown_ifclassq(ifq);
	}
	return;
}

int
pktsched_getqstats(struct ifclassq *ifq, u_int32_t gid, u_int32_t qid,
    struct if_ifclassq_stats *ifqs)
{
	int error = 0;

	IFCQ_LOCK_ASSERT_HELD(ifq);

	if (ifq->ifcq_type == PKTSCHEDT_FQ_CODEL) {
		/* Could be PKTSCHEDT_NONE */
		error = fq_if_getqstats_ifclassq(ifq, (uint8_t)gid, qid, ifqs);
	}

	return error;
}

void
pktsched_pkt_encap(pktsched_pkt_t *pkt, classq_pkt_t *cpkt)
{
	pkt->pktsched_pkt = *cpkt;
	pkt->pktsched_tail = *cpkt;
	pkt->pktsched_pcnt = 1;

	switch (cpkt->cp_ptype) {
	case QP_MBUF:
		pkt->pktsched_plen =
		    (uint32_t)m_pktlen(pkt->pktsched_pkt_mbuf);
		break;

#if SKYWALK
	case QP_PACKET:
		pkt->pktsched_plen = pkt->pktsched_pkt_kpkt->pkt_length;
		break;
#endif /* SKYWALK */

	default:
		VERIFY(0);
		/* NOTREACHED */
		__builtin_unreachable();
	}
}

void
pktsched_pkt_encap_chain(pktsched_pkt_t *pkt, classq_pkt_t *cpkt,
    classq_pkt_t *tail, uint32_t cnt, uint32_t bytes)
{
	pkt->pktsched_pkt = *cpkt;
	pkt->pktsched_tail = *tail;
	pkt->pktsched_pcnt = cnt;
	pkt->pktsched_plen = bytes;

	switch (cpkt->cp_ptype) {
	case QP_MBUF:
		break;

#if SKYWALK
	case QP_PACKET:
		break;
#endif /* SKYWALK */

	default:
		VERIFY(0);
		/* NOTREACHED */
		__builtin_unreachable();
	}
}

int
pktsched_clone_pkt(pktsched_pkt_t *pkt1, pktsched_pkt_t *pkt2)
{
	struct mbuf *m1, *m2;
#if SKYWALK
	struct __kern_packet *p1;
	kern_packet_t ph2;
	int err;
#endif /* SKYWALK */

	ASSERT(pkt1 != NULL);
	ASSERT(pkt1->pktsched_pkt_mbuf != NULL);
	ASSERT(pkt1->pktsched_pcnt == 1);

	/* allow in place clone, but make sure pkt2->pktsched_pkt won't leak */
	ASSERT((pkt1 == pkt2 && pkt1->pktsched_pkt_mbuf ==
	    pkt2->pktsched_pkt_mbuf) || (pkt1 != pkt2 &&
	    pkt2->pktsched_pkt_mbuf == NULL));

	switch (pkt1->pktsched_ptype) {
	case QP_MBUF:
		m1 = (struct mbuf *)pkt1->pktsched_pkt_mbuf;
		m2 = m_dup(m1, M_NOWAIT);
		if (__improbable(m2 == NULL)) {
			return ENOBUFS;
		}
		pkt2->pktsched_pkt_mbuf = m2;
		break;

#if SKYWALK
	case QP_PACKET:
		p1 = (struct __kern_packet *)pkt1->pktsched_pkt_kpkt;
		err = kern_packet_clone_nosleep(SK_PTR_ENCODE(p1,
		    METADATA_TYPE(p1), METADATA_SUBTYPE(p1)), &ph2,
		    KPKT_COPY_HEAVY);
		if (__improbable(err != 0)) {
			return err;
		}
		ASSERT(ph2 != 0);
		VERIFY(kern_packet_finalize(ph2) == 0);
		pkt2->pktsched_pkt_kpkt = SK_PTR_ADDR_KPKT(ph2);
		break;
#endif /* SKYWALK */

	default:
		VERIFY(0);
		/* NOTREACHED */
		__builtin_unreachable();
	}

	pkt2->pktsched_plen = pkt1->pktsched_plen;
	pkt2->pktsched_ptype = pkt1->pktsched_ptype;
	pkt2->pktsched_tail = pkt2->pktsched_pkt;
	pkt2->pktsched_pcnt = 1;
	return 0;
}

void
pktsched_corrupt_packet(pktsched_pkt_t *pkt)
{
	struct mbuf *m = NULL;
	uint8_t *data = NULL;
	uint32_t data_len = 0;
	uint32_t rand32, rand_off, rand_bit;
#if SKYWALK
	struct __kern_packet *p = NULL;
#endif /* SKYWALK */

	switch (pkt->pktsched_ptype) {
	case QP_MBUF:
		m = pkt->pktsched_pkt_mbuf;
		data = mtod(m, uint8_t *);
		data_len = m->m_pkthdr.len;
		break;
#if SKYWALK
	case QP_PACKET:
		p = pkt->pktsched_pkt_kpkt;
		if (p->pkt_pflags & PKT_F_MBUF_DATA) {
			m = p->pkt_mbuf;
			data = mtod(m, uint8_t *);
			data_len = m->m_pkthdr.len;
		} else {
			MD_BUFLET_ADDR_DLEN(p, data, data_len);
		}
		break;
#endif /* SKYWALK */

	default:
		/* NOTREACHED */
		VERIFY(0);
		__builtin_unreachable();
	}

	read_frandom(&rand32, sizeof(rand32));
	rand_bit = rand32 & 0x7;
	rand_off = (rand32 >> 3) % data_len;
	data[rand_off] ^= (uint8_t)(1 << rand_bit);
}

void
pktsched_free_pkt(pktsched_pkt_t *pkt)
{
	uint32_t cnt = pkt->pktsched_pcnt;
	ASSERT(cnt != 0);

	switch (pkt->pktsched_ptype) {
	case QP_MBUF: {
		struct mbuf *m;

		m = pkt->pktsched_pkt_mbuf;
		if (cnt == 1) {
			VERIFY(m->m_nextpkt == NULL);
		} else {
			VERIFY(m->m_nextpkt != NULL);
		}
		m_freem_list(m);
		break;
	}
#if SKYWALK
	case QP_PACKET: {
		struct __kern_packet *kpkt;
		int pcnt = 0;

		kpkt = pkt->pktsched_pkt_kpkt;
		if (cnt == 1) {
			VERIFY(kpkt->pkt_nextpkt == NULL);
		} else {
			VERIFY(kpkt->pkt_nextpkt != NULL);
		}
		pp_free_packet_chain(kpkt, &pcnt);
		VERIFY(cnt == (uint32_t)pcnt);
		break;
	}
#endif /* SKYWALK */

	default:
		VERIFY(0);
		/* NOTREACHED */
		__builtin_unreachable();
	}
	pkt->pktsched_pkt = CLASSQ_PKT_INITIALIZER(pkt->pktsched_pkt);
	pkt->pktsched_tail = CLASSQ_PKT_INITIALIZER(pkt->pktsched_tail);
	pkt->pktsched_plen = 0;
	pkt->pktsched_pcnt = 0;
}

void
pktsched_drop_pkt(pktsched_pkt_t *pkt, drop_reason_t reason, const char *funcname,
    uint16_t linenum, uint16_t flags)
{
	if (__probable(droptap_total_tap_count == 0)) {
		pktsched_free_pkt(pkt);
		return;
	}

	uint32_t cnt = pkt->pktsched_pcnt;
	ASSERT(cnt != 0);

	switch (pkt->pktsched_ptype) {
	case QP_MBUF: {
		struct mbuf *m;

		m = pkt->pktsched_pkt_mbuf;
		if (cnt == 1) {
			VERIFY(m->m_nextpkt == NULL);
		} else {
			VERIFY(m->m_nextpkt != NULL);
		}
		m_drop_list(m, flags | DROPTAP_FLAG_DIR_OUT, reason, funcname, linenum);
		break;
	}
#if SKYWALK
	case QP_PACKET: {
		struct __kern_packet *kpkt;

		kpkt = pkt->pktsched_pkt_kpkt;
		if (cnt == 1) {
			VERIFY(kpkt->pkt_nextpkt == NULL);
		} else {
			VERIFY(kpkt->pkt_nextpkt != NULL);
		}
		droptap_output_packet(SK_PKT2PH(kpkt), reason, funcname, linenum,
		    flags, NULL, kpkt->pkt_qum.qum_pid, NULL, -1, NULL, 0, 0);
		break;
	}
#endif /* SKYWALK */

	default:
		VERIFY(0);
		/* NOTREACHED */
		__builtin_unreachable();
	}

	pktsched_free_pkt(pkt);
}

mbuf_svc_class_t
pktsched_get_pkt_svc(pktsched_pkt_t *pkt)
{
	mbuf_svc_class_t svc = MBUF_SC_UNSPEC;

	switch (pkt->pktsched_ptype) {
	case QP_MBUF:
		svc = m_get_service_class(pkt->pktsched_pkt_mbuf);
		break;

#if SKYWALK
	case QP_PACKET:
		svc = pkt->pktsched_pkt_kpkt->pkt_svc_class;
		break;
#endif /* SKYWALK */

	default:
		VERIFY(0);
		/* NOTREACHED */
		__builtin_unreachable();
	}

	return svc;
}

void
pktsched_get_pkt_vars(pktsched_pkt_t *pkt, volatile uint32_t **flags,
    uint64_t **timestamp, uint32_t *flowid, uint8_t *flowsrc, uint8_t *proto,
    uint32_t *comp_gencnt, uint64_t *pkt_tx_time)
{
	switch (pkt->pktsched_ptype) {
	case QP_MBUF: {
		struct pkthdr *pkth = &(pkt->pktsched_pkt_mbuf->m_pkthdr);

		if (flags != NULL) {
			*flags = &pkth->pkt_flags;
		}
		if (timestamp != NULL) {
			*timestamp = &pkth->pkt_timestamp;
		}
		if (flowid != NULL) {
			*flowid = pkth->pkt_flowid;
		}
		if (flowsrc != NULL) {
			*flowsrc = pkth->pkt_flowsrc;
		}
		if (proto != NULL) {
			/*
			 * rdar://100524205 - We want to use the pkt_ext_flags
			 * to denote QUIC packets, but AQM is already written in
			 * such a way where IPPROTO_QUIC is used to denote QUIC
			 * packets.
			 */
			if (pkth->pkt_ext_flags & PKTF_EXT_QUIC) {
				*proto = IPPROTO_QUIC;
			} else {
				*proto = pkth->pkt_proto;
			}
		}
		if (comp_gencnt != NULL) {
			*comp_gencnt = pkth->comp_gencnt;
		}
		if (pkt_tx_time != NULL) {
			struct m_tag *tag;
			tag = m_tag_locate(pkt->pktsched_pkt_mbuf, KERNEL_MODULE_TAG_ID,
			    KERNEL_TAG_TYPE_AQM);
			if (__improbable(tag != NULL)) {
				*pkt_tx_time = *(uint64_t *)tag->m_tag_data;
			} else {
				*pkt_tx_time = 0;
			}
		}

		break;
	}

#if SKYWALK
	case QP_PACKET: {
		struct __kern_packet *kp = pkt->pktsched_pkt_kpkt;

		if (flags != NULL) {
			/* use lower-32 bit for common flags */
			*flags = &kp->pkt_pflags32;
		}
		if (timestamp != NULL) {
			*timestamp = &kp->pkt_timestamp;
		}
		if (flowid != NULL) {
			*flowid = kp->pkt_flow_token;
		}
		if (flowsrc != NULL) {
			*flowsrc = (uint8_t)kp->pkt_flowsrc_type;
		}
		if (proto != NULL) {
			*proto = kp->pkt_transport_protocol;
		}
		if (comp_gencnt != NULL) {
			*comp_gencnt = kp->pkt_comp_gencnt;
		}
		if (pkt_tx_time != NULL && (kp->pkt_pflags & PKT_F_OPT_TX_TIMESTAMP) != 0) {
			*pkt_tx_time = kp->pkt_com_opt->__po_pkt_tx_time;
		}

		break;
	}
#endif /* SKYWALK */

	default:
		VERIFY(0);
		/* NOTREACHED */
		__builtin_unreachable();
	}
}

struct flowadv_fcentry *
pktsched_alloc_fcentry(pktsched_pkt_t *pkt, struct ifnet *ifp, int how)
{
#pragma unused(ifp)
	struct flowadv_fcentry *fce = NULL;

	switch (pkt->pktsched_ptype) {
	case QP_MBUF: {
		struct mbuf *m = pkt->pktsched_pkt_mbuf;

		fce = flowadv_alloc_entry(how);
		if (fce == NULL) {
			break;
		}

		_CASSERT(sizeof(m->m_pkthdr.pkt_flowid) ==
		    sizeof(fce->fce_flowid));

		fce->fce_flowsrc_type = m->m_pkthdr.pkt_flowsrc;
		fce->fce_flowid = m->m_pkthdr.pkt_flowid;
#if SKYWALK
		_CASSERT(sizeof(m->m_pkthdr.pkt_mpriv_srcid) ==
		    sizeof(fce->fce_flowsrc_token));
		_CASSERT(sizeof(m->m_pkthdr.pkt_mpriv_fidx) ==
		    sizeof(fce->fce_flowsrc_fidx));

		if (fce->fce_flowsrc_type == FLOWSRC_CHANNEL) {
			fce->fce_flowsrc_fidx = m->m_pkthdr.pkt_mpriv_fidx;
			fce->fce_flowsrc_token = m->m_pkthdr.pkt_mpriv_srcid;
			fce->fce_ifp = ifp;
		}
#endif /* SKYWALK */
		break;
	}

#if SKYWALK
	case QP_PACKET: {
		struct __kern_packet *kp = pkt->pktsched_pkt_kpkt;

		fce = flowadv_alloc_entry(how);
		if (fce == NULL) {
			break;
		}

		_CASSERT(sizeof(fce->fce_flowid) ==
		    sizeof(kp->pkt_flow_token));
		_CASSERT(sizeof(fce->fce_flowsrc_fidx) ==
		    sizeof(kp->pkt_flowsrc_fidx));
		_CASSERT(sizeof(fce->fce_flowsrc_token) ==
		    sizeof(kp->pkt_flowsrc_token));

		ASSERT(kp->pkt_pflags & PKT_F_FLOW_ADV);
		fce->fce_flowsrc_type = kp->pkt_flowsrc_type;
		fce->fce_flowid = kp->pkt_flow_token;
		fce->fce_flowsrc_fidx = kp->pkt_flowsrc_fidx;
		fce->fce_flowsrc_token = kp->pkt_flowsrc_token;
		fce->fce_ifp = ifp;
		break;
	}
#endif /* SKYWALK */

	default:
		VERIFY(0);
		/* NOTREACHED */
		__builtin_unreachable();
	}

	return fce;
}

uint32_t *
pktsched_get_pkt_sfb_vars(pktsched_pkt_t *pkt, uint32_t **sfb_flags)
{
	uint32_t *hashp = NULL;

	switch (pkt->pktsched_ptype) {
	case QP_MBUF: {
		struct pkthdr *pkth = &(pkt->pktsched_pkt_mbuf->m_pkthdr);

		_CASSERT(sizeof(pkth->pkt_mpriv_hash) == sizeof(uint32_t));
		_CASSERT(sizeof(pkth->pkt_mpriv_flags) == sizeof(uint32_t));
		*sfb_flags = &pkth->pkt_mpriv_flags;
		hashp = &pkth->pkt_mpriv_hash;
		break;
	}

#if SKYWALK
	case QP_PACKET: {
		struct __kern_packet *kp = pkt->pktsched_pkt_kpkt;

		_CASSERT(sizeof(kp->pkt_classq_hash) == sizeof(uint32_t));
		_CASSERT(sizeof(kp->pkt_classq_flags) == sizeof(uint32_t));
		*sfb_flags = &kp->pkt_classq_flags;
		hashp = &kp->pkt_classq_hash;
		break;
	}
#endif /* SKYWALK */

	default:
		VERIFY(0);
		/* NOTREACHED */
		__builtin_unreachable();
	}

	return hashp;
}

static int
pktsched_mbuf_mark_ecn(struct mbuf* m)
{
	struct mbuf     *m0;
	void            *__single hdr;
	int             af;
	uint8_t         ipv;

	hdr = m->m_pkthdr.pkt_hdr;
	/* verify that hdr is within the mbuf data */
	for (m0 = m; m0 != NULL; m0 = m0->m_next) {
		if (((caddr_t)hdr >= m_mtod_current(m0)) &&
		    ((caddr_t)hdr < m_mtod_current(m0) + m0->m_len)) {
			break;
		}
	}
	if (m0 == NULL) {
		return EINVAL;
	}
	ipv = IP_VHL_V(*(uint8_t *)hdr);
	if (ipv == 4) {
		af = AF_INET;
	} else if (ipv == 6) {
		af = AF_INET6;
	} else {
		af = AF_UNSPEC;
	}

	switch (af) {
	case AF_INET: {
		struct ip *__single ip = (struct ip *)(void *)hdr;
		uint8_t otos;
		int sum;

		if (((uintptr_t)ip + sizeof(*ip)) >
		    ((uintptr_t)mbuf_datastart(m0) + mbuf_maxlen(m0))) {
			return EINVAL;    /* out of bounds */
		}
		if ((ip->ip_tos & IPTOS_ECN_MASK) == IPTOS_ECN_NOTECT) {
			return EINVAL;    /* not-ECT */
		}
		if ((ip->ip_tos & IPTOS_ECN_MASK) == IPTOS_ECN_CE) {
			return 0;    /* already marked */
		}
		/*
		 * ecn-capable but not marked,
		 * mark CE and update checksum
		 */
		otos = ip->ip_tos;
		ip->ip_tos |= IPTOS_ECN_CE;
		/*
		 * update checksum (from RFC1624) only if hw
		 * checksum is not supported.
		 *         HC' = ~(~HC + ~m + m')
		 */
		if ((m->m_pkthdr.csum_flags & CSUM_DELAY_IP) == 0) {
			sum = ~ntohs(ip->ip_sum) & 0xffff;
			sum += (~otos & 0xffff) + ip->ip_tos;
			sum = (sum >> 16) + (sum & 0xffff);
			sum += (sum >> 16); /* add carry */
			ip->ip_sum = htons(~sum & 0xffff);
		}
		return 0;
	}
	case AF_INET6: {
		struct ip6_hdr *__single ip6 = (struct ip6_hdr *)(void *)hdr;
		u_int32_t flowlabel;

		if (((uintptr_t)ip6 + sizeof(*ip6)) >
		    ((uintptr_t)mbuf_datastart(m0) + mbuf_maxlen(m0))) {
			return EINVAL;    /* out of bounds */
		}
		flowlabel = ntohl(ip6->ip6_flow);
		if ((flowlabel & (IPTOS_ECN_MASK << 20)) ==
		    (IPTOS_ECN_NOTECT << 20)) {
			return EINVAL;    /* not-ECT */
		}
		if ((flowlabel & (IPTOS_ECN_MASK << 20)) ==
		    (IPTOS_ECN_CE << 20)) {
			return 0;    /* already marked */
		}
		/*
		 * ecn-capable but not marked,  mark CE
		 */
		flowlabel |= (IPTOS_ECN_CE << 20);
		ip6->ip6_flow = htonl(flowlabel);
		return 0;
	}
	default:
		return EPROTONOSUPPORT;
	}
}

static int
pktsched_kpkt_mark_ecn(struct __kern_packet *kpkt)
{
	uint8_t ipv = 0, *l3_hdr;

	if ((kpkt->pkt_qum_qflags & QUM_F_FLOW_CLASSIFIED) != 0) {
		uint32_t l3_len = 0;
		ipv = kpkt->pkt_flow_ip_ver;
		l3_len = kpkt->pkt_length - kpkt->pkt_l2_len;
		l3_hdr = __unsafe_forge_bidi_indexable(uint8_t *, kpkt->pkt_flow_ip_hdr, l3_len);
	} else {
		uint8_t *pkt_buf;
		uint32_t bdlen, bdlim, bdoff;
		MD_BUFLET_ADDR_ABS_DLEN(kpkt, pkt_buf, bdlen, bdlim, bdoff);

		/* takes care of both IPv4 and IPv6 */
		l3_hdr = pkt_buf + kpkt->pkt_headroom + kpkt->pkt_l2_len;
		ipv = IP_VHL_V(*(uint8_t *)l3_hdr);
		if (ipv == 4) {
			ipv = IPVERSION;
		} else if (ipv == 6) {
			ipv = IPV6_VERSION;
		} else {
			ipv = 0;
		}
	}

	switch (ipv) {
	case IPVERSION: {
		uint8_t otos;
		int sum;

		struct ip *ip = (struct ip *)(void *)l3_hdr;
		if ((ip->ip_tos & IPTOS_ECN_MASK) == IPTOS_ECN_NOTECT) {
			return EINVAL;    /* not-ECT */
		}
		if ((ip->ip_tos & IPTOS_ECN_MASK) == IPTOS_ECN_CE) {
			return 0;    /* already marked */
		}
		/*
		 * ecn-capable but not marked,
		 * mark CE and update checksum
		 */
		otos = ip->ip_tos;
		ip->ip_tos |= IPTOS_ECN_CE;

		sum = ~ntohs(ip->ip_sum) & 0xffff;
		sum += (~otos & 0xffff) + ip->ip_tos;
		sum = (sum >> 16) + (sum & 0xffff);
		sum += (sum >> 16); /* add carry */
		ip->ip_sum = htons(~sum & 0xffff);

		return 0;
	}
	case IPV6_VERSION: {
		struct ip6_hdr *ip6 = (struct ip6_hdr *)(void *)l3_hdr;
		u_int32_t flowlabel;
		flowlabel = ntohl(ip6->ip6_flow);
		if ((flowlabel & (IPTOS_ECN_MASK << 20)) ==
		    (IPTOS_ECN_NOTECT << 20)) {
			return EINVAL;    /* not-ECT */
		}
		if ((flowlabel & (IPTOS_ECN_MASK << 20)) ==
		    (IPTOS_ECN_CE << 20)) {
			return 0;    /* already marked */
		}
		/*
		 * ecn-capable but not marked, mark CE
		 */
		flowlabel |= (IPTOS_ECN_CE << 20);
		ip6->ip6_flow = htonl(flowlabel);

		return 0;
	}
	default:
		return EPROTONOSUPPORT;
	}
}

int
pktsched_mark_ecn(pktsched_pkt_t *pkt)
{
	switch (pkt->pktsched_ptype) {
	case QP_MBUF:
		return pktsched_mbuf_mark_ecn(pkt->pktsched_pkt_mbuf);
	case QP_PACKET:
		return pktsched_kpkt_mark_ecn(pkt->pktsched_pkt_kpkt);
	default:
		VERIFY(0);
		/* NOTREACHED */
		__builtin_unreachable();
	}
}

boolean_t
pktsched_is_pkt_l4s(pktsched_pkt_t *pkt)
{
	switch (pkt->pktsched_ptype) {
	case QP_MBUF: {
		struct pkthdr *pkth = &(pkt->pktsched_pkt_mbuf->m_pkthdr);
		return (pkth->pkt_ext_flags & PKTF_EXT_L4S) != 0;
	}
	case QP_PACKET: {
		struct __kern_packet *kp = pkt->pktsched_pkt_kpkt;
		return (kp->pkt_pflags & PKT_F_L4S) != 0;
	}

	default:
		VERIFY(0);
		/* NOTREACHED */
		__builtin_unreachable();
	}
	return FALSE;
}

struct aqm_tag_container {
	struct m_tag            aqm_m_tag;
	uint64_t                aqm_tag;
};

static struct  m_tag *
m_tag_kalloc_aqm(u_int32_t id, u_int16_t type, uint16_t len, int wait)
{
	struct aqm_tag_container *tag_container;
	struct m_tag *tag = NULL;

	assert3u(id, ==, KERNEL_MODULE_TAG_ID);
	assert3u(type, ==, KERNEL_TAG_TYPE_AQM);
	assert3u(len, ==, sizeof(uint64_t));

	if (len != sizeof(uint64_t)) {
		return NULL;
	}

	tag_container = kalloc_type(struct aqm_tag_container, wait | M_ZERO);
	if (tag_container != NULL) {
		tag = &tag_container->aqm_m_tag;

		assert3p(tag, ==, tag_container);

		M_TAG_INIT(tag, id, type, len, &tag_container->aqm_tag, NULL);
	}

	return tag;
}

static void
m_tag_kfree_aqm(struct m_tag *tag)
{
	struct aqm_tag_container *__single tag_container = (struct aqm_tag_container *)tag;

	assert3u(tag->m_tag_len, ==, sizeof(uint64_t));

	kfree_type(struct aqm_tag_container, tag_container);
}

void
pktsched_register_m_tag(void)
{
	int error;

	error = m_register_internal_tag_type(KERNEL_TAG_TYPE_AQM, sizeof(uint64_t),
	    m_tag_kalloc_aqm, m_tag_kfree_aqm);

	assert3u(error, ==, 0);
}
