/*
 * Copyright (c) 2004-2024 Apple Inc. All rights reserved.
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

#include <sys/param.h>  /* for definition of NULL */
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <libkern/OSAtomic.h>

#include <machine/endian.h>

#define _IP_VHL
#include <net/if_var.h>
#include <net/route.h>
#include <net/kpi_protocol.h>
#include <net/net_api_stats.h>
#if SKYWALK
#include <skywalk/lib//net_filter_event.h>
#endif /* SKYWALK */

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/in6_var.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_var.h>
#include <netinet6/ip6_var.h>
#include <netinet/kpi_ipfilter_var.h>

#include <stdbool.h>

#if SKYWALK
#include <skywalk/core/skywalk_var.h>
#endif /* SKYWALK */

/*
 * kipf_lock and kipf_ref protect the linkage of the list of IP filters
 * An IP filter can be removed only when kipf_ref is zero
 * If an IP filter cannot be removed because kipf_ref is not null, then
 * the IP filter is marjed and kipf_delayed_remove is set so that when
 * kipf_ref eventually goes down to zero, the IP filter is removed
 */
static LCK_GRP_DECLARE(kipf_lock_grp, "IP Filter");
static LCK_MTX_DECLARE(kipf_lock, &kipf_lock_grp);
static u_int32_t kipf_ref = 0;
static u_int32_t kipf_delayed_remove = 0;
u_int32_t kipf_count = 0;

__private_extern__ struct ipfilter_list ipv4_filters = TAILQ_HEAD_INITIALIZER(ipv4_filters);
__private_extern__ struct ipfilter_list ipv6_filters = TAILQ_HEAD_INITIALIZER(ipv6_filters);
__private_extern__ struct ipfilter_list tbr_filters = TAILQ_HEAD_INITIALIZER(tbr_filters);

#undef ipf_addv4
#undef ipf_addv6
extern errno_t ipf_addv4(const struct ipf_filter *filter,
    ipfilter_t *filter_ref);
extern errno_t ipf_addv6(const struct ipf_filter *filter,
    ipfilter_t *filter_ref);

static errno_t ipf_add(const struct ipf_filter *filter,
    ipfilter_t *filter_ref, struct ipfilter_list *head, bool is_internal);

#if SKYWALK
static bool net_check_compatible_ipf(void);
#endif /* SKYWALK */

__private_extern__ void
ipf_ref(void)
{
	lck_mtx_lock(&kipf_lock);
	if (os_inc_overflow(&kipf_ref)) {
		panic("kipf_ref overflow");
	}
	lck_mtx_unlock(&kipf_lock);
}

__private_extern__ void
ipf_unref(void)
{
	lck_mtx_lock(&kipf_lock);

	if (os_dec_overflow(&kipf_ref)) {
		panic("kipf_ref underflow");
	}

	if (kipf_ref == 0 && kipf_delayed_remove != 0) {
		struct ipfilter *filter;

		while ((filter = TAILQ_FIRST(&tbr_filters))) {
			VERIFY(OSDecrementAtomic64(&net_api_stats.nas_ipf_add_count) > 0);
			if (filter->ipf_flags & IPFF_INTERNAL) {
				VERIFY(OSDecrementAtomic64(&net_api_stats.nas_ipf_add_os_count) > 0);
			}

			ipf_detach_func ipf_detach = filter->ipf_filter.ipf_detach;
			void *__single cookie = filter->ipf_filter.cookie;

			TAILQ_REMOVE(filter->ipf_head, filter, ipf_link);
			TAILQ_REMOVE(&tbr_filters, filter, ipf_tbr);
			kipf_delayed_remove--;

			if (ipf_detach) {
				lck_mtx_unlock(&kipf_lock);
				ipf_detach(cookie);
				lck_mtx_lock(&kipf_lock);
				/* In case some filter got to run while we released the lock */
				if (kipf_ref != 0) {
					break;
				}
			}
		}
	}
#if SKYWALK
	if (kernel_is_macos_or_server()) {
		net_filter_event_mark(NET_FILTER_EVENT_IP,
		    net_check_compatible_ipf());
	}
#endif /* SKYWALK */
	lck_mtx_unlock(&kipf_lock);
}

static errno_t
ipf_add(
	const struct ipf_filter *filter,
	ipfilter_t *filter_ref,
	struct ipfilter_list *head,
	bool is_internal)
{
	struct ipfilter *new_filter;
	if (filter->name == NULL || (filter->ipf_input == NULL && filter->ipf_output == NULL)) {
		return EINVAL;
	}

	new_filter = kalloc_type(struct ipfilter, Z_WAITOK | Z_NOFAIL);

	lck_mtx_lock(&kipf_lock);
	new_filter->ipf_filter = *filter;
	new_filter->ipf_head = head;

	TAILQ_INSERT_HEAD(head, new_filter, ipf_link);

	OSIncrementAtomic64(&net_api_stats.nas_ipf_add_count);
	INC_ATOMIC_INT64_LIM(net_api_stats.nas_ipf_add_total);
	if (is_internal) {
		new_filter->ipf_flags = IPFF_INTERNAL;
		OSIncrementAtomic64(&net_api_stats.nas_ipf_add_os_count);
		INC_ATOMIC_INT64_LIM(net_api_stats.nas_ipf_add_os_total);
	}
#if SKYWALK
	if (kernel_is_macos_or_server()) {
		net_filter_event_mark(NET_FILTER_EVENT_IP,
		    net_check_compatible_ipf());
	}
#endif /* SKYWALK */

	lck_mtx_unlock(&kipf_lock);

	*filter_ref = (ipfilter_t)new_filter;

	/* This will force TCP to re-evaluate its use of TSO */
	OSAddAtomic(1, &kipf_count);
	routegenid_update();

	return 0;
}

errno_t
ipf_addv4_internal(
	const struct ipf_filter *filter,
	ipfilter_t *filter_ref)
{
	return ipf_add(filter, filter_ref, &ipv4_filters, true);
}

errno_t
ipf_addv4(
	const struct ipf_filter *filter,
	ipfilter_t *filter_ref)
{
	return ipf_add(filter, filter_ref, &ipv4_filters, false);
}

errno_t
ipf_addv6_internal(
	const struct ipf_filter *filter,
	ipfilter_t *filter_ref)
{
	return ipf_add(filter, filter_ref, &ipv6_filters, true);
}

errno_t
ipf_addv6(
	const struct ipf_filter *filter,
	ipfilter_t *filter_ref)
{
	return ipf_add(filter, filter_ref, &ipv6_filters, false);
}

static errno_t
ipf_input_detached(void *cookie, mbuf_t *data, int offset, u_int8_t protocol)
{
#pragma unused(cookie, data, offset, protocol)

#if DEBUG
	printf("ipf_input_detached\n");
#endif /* DEBUG */

	return 0;
}

static errno_t
ipf_output_detached(void *cookie, mbuf_t *data, ipf_pktopts_t options)
{
#pragma unused(cookie, data, options)

#if DEBUG
	printf("ipf_output_detached\n");
#endif /* DEBUG */

	return 0;
}

errno_t
ipf_remove(
	ipfilter_t filter_ref)
{
	struct ipfilter *match = (struct ipfilter *)filter_ref;
	struct ipfilter_list *head;

	if (match == 0 || (match->ipf_head != &ipv4_filters && match->ipf_head != &ipv6_filters)) {
		return EINVAL;
	}

	head = match->ipf_head;

	lck_mtx_lock(&kipf_lock);
	TAILQ_FOREACH(match, head, ipf_link) {
		if (match == (struct ipfilter *)filter_ref) {
			ipf_detach_func ipf_detach = match->ipf_filter.ipf_detach;
			void *__single cookie = match->ipf_filter.cookie;

			/*
			 * Cannot detach when they are filters running
			 */
			if (kipf_ref) {
				kipf_delayed_remove++;
				TAILQ_INSERT_TAIL(&tbr_filters, match, ipf_tbr);
				match->ipf_filter.ipf_input = ipf_input_detached;
				match->ipf_filter.ipf_output = ipf_output_detached;
				lck_mtx_unlock(&kipf_lock);
			} else {
				VERIFY(OSDecrementAtomic64(&net_api_stats.nas_ipf_add_count) > 0);
				if (match->ipf_flags & IPFF_INTERNAL) {
					VERIFY(OSDecrementAtomic64(&net_api_stats.nas_ipf_add_os_count) > 0);
				}

				TAILQ_REMOVE(head, match, ipf_link);
				lck_mtx_unlock(&kipf_lock);

				if (ipf_detach) {
					ipf_detach(cookie);
				}
				kfree_type(struct ipfilter, match);

				/* This will force TCP to re-evaluate its use of TSO */
				OSAddAtomic(-1, &kipf_count);
				routegenid_update();
			}
			return 0;
		}
	}
#if SKYWALK
	if (kernel_is_macos_or_server()) {
		net_filter_event_mark(NET_FILTER_EVENT_IP,
		    net_check_compatible_ipf());
	}
#endif /* SKYWALK */

	lck_mtx_unlock(&kipf_lock);

	return ENOENT;
}

int log_for_en1 = 0;

errno_t
ipf_inject_input(
	mbuf_t data,
	ipfilter_t filter_ref)
{
	struct mbuf *m = (struct mbuf *)data;
	struct m_tag *mtag = 0;
	struct ip *ip = mtod(m, struct ip *);
	struct ip6_hdr *ip6;
	u_int8_t        vers;
	int hlen;
	errno_t error = 0;
	protocol_family_t proto;
	struct in_ifaddr *ia = NULL;
	struct in_addr *pkt_dst = NULL;
	struct in6_ifaddr *ia6 = NULL;
	struct sockaddr_in6 pkt_dst6;

	vers = IP_VHL_V(ip->ip_vhl);

	switch (vers) {
	case 4:
		proto = PF_INET;
		break;
	case 6:
		proto = PF_INET6;
		break;
	default:
		error = ENOTSUP;
		goto done;
	}

	if (filter_ref == 0 && m->m_pkthdr.rcvif == 0) {
		/*
		 * Search for interface with the local address
		 */
		switch (proto) {
		case PF_INET:
			pkt_dst = &ip->ip_dst;
			lck_rw_lock_shared(&in_ifaddr_rwlock);
			TAILQ_FOREACH(ia, INADDR_HASH(pkt_dst->s_addr), ia_hash) {
				if (IA_SIN(ia)->sin_addr.s_addr == pkt_dst->s_addr) {
					m->m_pkthdr.rcvif = ia->ia_ifp;
					break;
				}
			}
			lck_rw_done(&in_ifaddr_rwlock);
			break;

		case PF_INET6:
			ip6 = mtod(m, struct ip6_hdr *);
			pkt_dst6.sin6_addr = ip6->ip6_dst;
			lck_rw_lock_shared(&in6_ifaddr_rwlock);
			TAILQ_FOREACH(ia6, IN6ADDR_HASH(&pkt_dst6.sin6_addr), ia6_hash) {
				if (IN6_ARE_ADDR_EQUAL(&ia6->ia_addr.sin6_addr, &pkt_dst6.sin6_addr)) {
					m->m_pkthdr.rcvif = ia6->ia_ifp;
					break;
				}
			}
			lck_rw_done(&in6_ifaddr_rwlock);
			break;

		default:
			break;
		}

		/*
		 * If none found, fallback to loopback
		 */
		if (m->m_pkthdr.rcvif == NULL) {
			m->m_pkthdr.rcvif = lo_ifp;
		}

		m->m_pkthdr.csum_data = 0;
		m->m_pkthdr.csum_flags = 0;
		if (vers == 4) {
			hlen = IP_VHL_HL(ip->ip_vhl) << 2;
			ip->ip_sum = 0;
			ip->ip_sum = in_cksum(m, hlen);
		}
	}
	if (filter_ref != 0) {
		mtag = m_tag_create(KERNEL_MODULE_TAG_ID, KERNEL_TAG_TYPE_IPFILT,
		    sizeof(ipfilter_t), M_NOWAIT, m);
		if (mtag == NULL) {
			error = ENOMEM;
			goto done;
		}
		*(ipfilter_t *)(mtag->m_tag_data) = filter_ref;
		m_tag_prepend(m, mtag);
	}

	error = proto_inject(proto, data);

done:
	return error;
}

static errno_t
ipf_injectv4_out(mbuf_t data, ipfilter_t filter_ref, ipf_pktopts_t options)
{
	struct route ro;
	struct ip *ip;
	struct mbuf *m = (struct mbuf *)data;
	errno_t error = 0;
	struct m_tag *mtag = NULL;
	struct ip_moptions *imo = NULL;
	struct ip_out_args ipoa;

	bzero(&ipoa, sizeof(ipoa));
	ipoa.ipoa_boundif = IFSCOPE_NONE;
	ipoa.ipoa_sotc = SO_TC_UNSPEC;
	ipoa.ipoa_netsvctype = _NET_SERVICE_TYPE_UNSPEC;

	/* Make the IP header contiguous in the mbuf */
	if ((size_t)m->m_len < sizeof(struct ip)) {
		m = m_pullup(m, sizeof(struct ip));
		if (m == NULL) {
			return ENOMEM;
		}
	}
	ip = mtod(m, struct ip *);

	if (filter_ref != 0) {
		mtag = m_tag_create(KERNEL_MODULE_TAG_ID,
		    KERNEL_TAG_TYPE_IPFILT, sizeof(ipfilter_t), M_NOWAIT, m);
		if (mtag == NULL) {
			m_freem(m);
			return ENOMEM;
		}
		*(ipfilter_t *)(mtag->m_tag_data) = filter_ref;
		m_tag_prepend(m, mtag);
	}

	if (options != NULL && (options->ippo_flags & IPPOF_MCAST_OPTS) &&
	    (imo = ip_allocmoptions(Z_NOWAIT)) != NULL) {
		imo->imo_multicast_ifp = options->ippo_mcast_ifnet;
		imo->imo_multicast_ttl = options->ippo_mcast_ttl;
		imo->imo_multicast_loop = (u_char)options->ippo_mcast_loop;
	}

	if (options != NULL) {
		if (options->ippo_flags & IPPOF_SELECT_SRCIF) {
			ipoa.ipoa_flags |= IPOAF_SELECT_SRCIF;
		}
		if (options->ippo_flags & IPPOF_BOUND_IF) {
			ipoa.ipoa_flags |= IPOAF_BOUND_IF;
			ipoa.ipoa_boundif = options->ippo_flags >>
			    IPPOF_SHIFT_IFSCOPE;
		}
		if (options->ippo_flags & IPPOF_NO_IFT_CELLULAR) {
			ipoa.ipoa_flags |= IPOAF_NO_CELLULAR;
		}
		if (options->ippo_flags & IPPOF_BOUND_SRCADDR) {
			ipoa.ipoa_flags |= IPOAF_BOUND_SRCADDR;
		}
		if (options->ippo_flags & IPPOF_NO_IFF_EXPENSIVE) {
			ipoa.ipoa_flags |= IPOAF_NO_EXPENSIVE;
		}
		if (options->ippo_flags & IPPOF_NO_IFF_CONSTRAINED) {
			ipoa.ipoa_flags |= IPOAF_NO_CONSTRAINED;
		}
	}

	bzero(&ro, sizeof(struct route));

	/* Put ip_len and ip_off in host byte order, ip_output expects that */

#if BYTE_ORDER != BIG_ENDIAN
	NTOHS(ip->ip_len);
	NTOHS(ip->ip_off);
#endif

	/* Send; enforce source interface selection via IP_OUTARGS flag */
	error = ip_output(m, NULL, &ro,
	    IP_ALLOWBROADCAST | IP_RAWOUTPUT | IP_OUTARGS, imo, &ipoa);

	/* Release the route */
	ROUTE_RELEASE(&ro);

	if (imo != NULL) {
		IMO_REMREF(imo);
	}

	return error;
}

static errno_t
ipf_injectv6_out(mbuf_t data, ipfilter_t filter_ref, ipf_pktopts_t options)
{
	struct route_in6 ro;
	struct ip6_hdr *ip6;
	struct mbuf *m = (struct mbuf *)data;
	errno_t error = 0;
	struct m_tag *mtag = NULL;
	struct ip6_moptions *im6o = NULL;
	struct ip6_out_args ip6oa;

	bzero(&ip6oa, sizeof(ip6oa));
	ip6oa.ip6oa_boundif = IFSCOPE_NONE;
	ip6oa.ip6oa_sotc = SO_TC_UNSPEC;
	ip6oa.ip6oa_netsvctype = _NET_SERVICE_TYPE_UNSPEC;

	/* Make the IP header contiguous in the mbuf */
	if ((size_t)m->m_len < sizeof(struct ip6_hdr)) {
		m = m_pullup(m, sizeof(struct ip6_hdr));
		if (m == NULL) {
			return ENOMEM;
		}
	}
	ip6 = mtod(m, struct ip6_hdr *);

	if (filter_ref != 0) {
		mtag = m_tag_create(KERNEL_MODULE_TAG_ID,
		    KERNEL_TAG_TYPE_IPFILT, sizeof(ipfilter_t), M_NOWAIT, m);
		if (mtag == NULL) {
			m_freem(m);
			return ENOMEM;
		}
		*(ipfilter_t *)(mtag->m_tag_data) = filter_ref;
		m_tag_prepend(m, mtag);
	}

	if (options != NULL && (options->ippo_flags & IPPOF_MCAST_OPTS) &&
	    (im6o = ip6_allocmoptions(Z_NOWAIT)) != NULL) {
		im6o->im6o_multicast_ifp = options->ippo_mcast_ifnet;
		im6o->im6o_multicast_hlim = options->ippo_mcast_ttl;
		im6o->im6o_multicast_loop = (u_char)options->ippo_mcast_loop;
	}

	if (options != NULL) {
		if (options->ippo_flags & IPPOF_SELECT_SRCIF) {
			ip6oa.ip6oa_flags |= IP6OAF_SELECT_SRCIF;
		}
		if (options->ippo_flags & IPPOF_BOUND_IF) {
			ip6oa.ip6oa_flags |= IP6OAF_BOUND_IF;
			ip6oa.ip6oa_boundif = options->ippo_flags >>
			    IPPOF_SHIFT_IFSCOPE;
		}
		if (options->ippo_flags & IPPOF_NO_IFT_CELLULAR) {
			ip6oa.ip6oa_flags |= IP6OAF_NO_CELLULAR;
		}
		if (options->ippo_flags & IPPOF_BOUND_SRCADDR) {
			ip6oa.ip6oa_flags |= IP6OAF_BOUND_SRCADDR;
		}
		if (options->ippo_flags & IPPOF_NO_IFF_EXPENSIVE) {
			ip6oa.ip6oa_flags |= IP6OAF_NO_EXPENSIVE;
		}
		if (options->ippo_flags & IPPOF_NO_IFF_CONSTRAINED) {
			ip6oa.ip6oa_flags |= IP6OAF_NO_CONSTRAINED;
		}
	}

	bzero(&ro, sizeof(struct route_in6));

	/*
	 * Send  mbuf and ifscope information. Check for correctness
	 * of ifscope information is done while searching for a route in
	 * ip6_output.
	 */
	ip6_output_setsrcifscope(m, IFSCOPE_UNKNOWN, NULL);
	ip6_output_setdstifscope(m, IFSCOPE_UNKNOWN, NULL);
	error = ip6_output(m, NULL, &ro, IPV6_OUTARGS, im6o, NULL, &ip6oa);

	/* Release the route */
	ROUTE_RELEASE(&ro);

	if (im6o != NULL) {
		IM6O_REMREF(im6o);
	}

	return error;
}

errno_t
ipf_inject_output(
	mbuf_t data,
	ipfilter_t filter_ref,
	ipf_pktopts_t options)
{
	struct mbuf     *m = (struct mbuf *)data;
	u_int8_t        vers;
	errno_t         error = 0;

#if SKYWALK
	sk_protect_t protect = sk_async_transmit_protect();
#endif /* SKYWALK */

	/* Make one byte of the header contiguous in the mbuf */
	if (m->m_len < 1) {
		m = m_pullup(m, 1);
		if (m == NULL) {
			goto done;
		}
	}

	vers = (*(u_int8_t *)m_mtod(m)) >> 4;
	switch (vers) {
	case 4:
		error = ipf_injectv4_out(data, filter_ref, options);
		break;
	case 6:
		error = ipf_injectv6_out(data, filter_ref, options);
		break;
	default:
		m_freem(m);
		error = ENOTSUP;
		break;
	}

done:
#if SKYWALK
	sk_async_transmit_unprotect(protect);
#endif /* SKYWALK */

	return error;
}

__private_extern__ ipfilter_t
ipf_get_inject_filter(struct mbuf *m)
{
	ipfilter_t __single filter_ref = 0;
	struct m_tag *mtag;

	mtag = m_tag_locate(m, KERNEL_MODULE_TAG_ID, KERNEL_TAG_TYPE_IPFILT);
	if (mtag) {
		filter_ref = *(ipfilter_t *)(mtag->m_tag_data);

		m_tag_delete(m, mtag);
	}
	return filter_ref;
}

struct ipfilt_tag_container {
	struct m_tag    ipft_m_tag;
	ipfilter_t      ipft_filter_ref;
};

static struct m_tag *
m_tag_kalloc_ipfilt(u_int32_t id, u_int16_t type, uint16_t len, int wait)
{
	struct ipfilt_tag_container *tag_container;
	struct m_tag *tag = NULL;

	assert3u(id, ==, KERNEL_MODULE_TAG_ID);
	assert3u(type, ==, KERNEL_TAG_TYPE_IPFILT);
	assert3u(len, ==, sizeof(ipfilter_t));

	if (len != sizeof(ipfilter_t)) {
		return NULL;
	}

	tag_container = kalloc_type(struct ipfilt_tag_container, wait | M_ZERO);
	if (tag_container != NULL) {
		tag =  &tag_container->ipft_m_tag;

		assert3p(tag, ==, tag_container);

		M_TAG_INIT(tag, id, type, len, &tag_container->ipft_filter_ref, NULL);
	}

	return tag;
}

static void
m_tag_kfree_ipfilt(struct m_tag *tag)
{
	struct ipfilt_tag_container *tag_container = (struct ipfilt_tag_container *)tag;

	assert3u(tag->m_tag_len, ==, sizeof(ipfilter_t));

	kfree_type(struct ipfilt_tag_container, tag_container);
}

void
ipfilter_register_m_tag(void)
{
	int error;

	error = m_register_internal_tag_type(KERNEL_TAG_TYPE_IPFILT, sizeof(ipfilter_t),
	    m_tag_kalloc_ipfilt, m_tag_kfree_ipfilt);

	assert3u(error, ==, 0);
}

#if SKYWALK
bool
net_check_compatible_ipf(void)
{
	if (net_api_stats.nas_ipf_add_count > net_api_stats.nas_ipf_add_os_count) {
		return false;
	}
	return true;
}
#endif /* SKYWALK */
