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
/*	$FreeBSD: src/sys/netinet/ip_encap.c,v 1.1.2.2 2001/07/03 11:01:46 ume Exp $	*/
/*	$KAME: ip_encap.c,v 1.41 2001/03/15 08:35:08 itojun Exp $	*/

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
 * My grandfather said that there's a devil inside tunnelling technology...
 *
 * We have surprisingly many protocols that want packets with IP protocol
 * #4 or #41.  Here's a list of protocols that want protocol #41:
 *	RFC1933 configured tunnel
 *	RFC1933 automatic tunnel
 *	RFC2401 IPsec tunnel
 *	RFC2473 IPv6 generic packet tunnelling
 *	RFC2529 6over4 tunnel
 *	mobile-ip6 (uses RFC2473)
 *	6to4 tunnel
 * Here's a list of protocol that want protocol #4:
 *	RFC1853 IPv4-in-IPv4 tunnelling
 *	RFC2003 IPv4 encapsulation within IPv4
 *	RFC2344 reverse tunnelling for mobile-ip4
 *	RFC2401 IPsec tunnel
 * Well, what can I say.  They impose different en/decapsulation mechanism
 * from each other, so they need separate protocol handler.  The only one
 * we can easily determine by protocol # is IPsec, which always has
 * AH/ESP header right after outer IP header.
 *
 * So, clearly good old protosw does not work for protocol #4 and #41.
 * The code will let you match protocol via src/dst address pair.
 */
/* XXX is M_NETADDR correct? */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/mcache.h>
#include <sys/errno.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_encap.h>

#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/ip6protosw.h>

#include <net/net_osdep.h>
#include <net/sockaddr_utils.h>

#ifndef __APPLE__
#include <sys/kernel.h>
#include <sys/malloc.h>
MALLOC_DEFINE(M_NETADDR, "Export Host", "Export host address structure");
#endif

static void encap_add_locked(struct encaptab *);
static int mask_match(const struct encaptab *, const struct sockaddr *,
    const struct sockaddr *);
static void encap_fillarg(struct mbuf *, void *arg);

LIST_HEAD(, encaptab) encaptab = LIST_HEAD_INITIALIZER(&encaptab);

static LCK_GRP_DECLARE(encaptab_lock_grp, "encaptab lock");
static LCK_RW_DECLARE(encaptab_lock, &encaptab_lock_grp);

#if INET
void
encap4_input(struct mbuf *m, int off)
{
	int proto;
	struct ip *__single ip;
	struct sockaddr_in s, d;
	const struct protosw *psw;
	struct encaptab *__single ep, *__single match;
	int prio, matchprio;
	void *__single match_arg = NULL;

#ifndef __APPLE__
	va_start(ap, m);
	off = va_arg(ap, int);
	proto = va_arg(ap, int);
	va_end(ap);
#endif

	/* Expect 32-bit aligned data pointer on strict-align platforms */
	MBUF_STRICT_DATA_ALIGNMENT_CHECK_32(m);

	ip = mtod(m, struct ip *);
#ifdef __APPLE__
	proto = ip->ip_p;
#endif

	SOCKADDR_ZERO(&s, sizeof(s));
	s.sin_family = AF_INET;
	s.sin_len = sizeof(struct sockaddr_in);
	s.sin_addr = ip->ip_src;
	SOCKADDR_ZERO(&d, sizeof(d));
	d.sin_family = AF_INET;
	d.sin_len = sizeof(struct sockaddr_in);
	d.sin_addr = ip->ip_dst;

	match = NULL;
	matchprio = 0;

	lck_rw_lock_shared(&encaptab_lock);
	for (ep = LIST_FIRST(&encaptab); ep; ep = LIST_NEXT(ep, chain)) {
		if (ep->af != AF_INET) {
			continue;
		}
		if (ep->proto >= 0 && ep->proto != proto) {
			continue;
		}
		if (ep->func) {
			prio = (*ep->func)(m, off, proto, ep->arg);
		} else {
			/*
			 * it's inbound traffic, we need to match in reverse
			 * order
			 */
			prio = mask_match(ep, SA(&d), SA(&s));
		}

		/*
		 * We prioritize the matches by using bit length of the
		 * matches.  mask_match() and user-supplied matching function
		 * should return the bit length of the matches (for example,
		 * if both src/dst are matched for IPv4, 64 should be returned).
		 * 0 or negative return value means "it did not match".
		 *
		 * The question is, since we have two "mask" portion, we
		 * cannot really define total order between entries.
		 * For example, which of these should be preferred?
		 * mask_match() returns 48 (32 + 16) for both of them.
		 *	src=3ffe::/16, dst=3ffe:501::/32
		 *	src=3ffe:501::/32, dst=3ffe::/16
		 *
		 * We need to loop through all the possible candidates
		 * to get the best match - the search takes O(n) for
		 * n attachments (i.e. interfaces).
		 */
		if (prio <= 0) {
			continue;
		}
		if (prio > matchprio) {
			matchprio = prio;
			match = ep;
			psw = (const struct protosw *)match->psw;
			match_arg = ep->arg;
		}
	}
	lck_rw_unlock_shared(&encaptab_lock);

	if (match) {
		/* found a match, "match" has the best one */
		if (psw && psw->pr_input) {
			encap_fillarg(m, match_arg);
			(*psw->pr_input)(m, off);
		} else {
			m_freem(m);
		}
		return;
	}

	/* last resort: inject to raw socket */
	rip_input(m, off);
}
#endif

int
encap6_input(struct mbuf **mp, int *offp, int proto)
{
	mbuf_ref_t m = *mp;
	struct ip6_hdr *__single ip6;
	struct sockaddr_in6 s, d;
	const struct ip6protosw *__single psw;
	struct encaptab *__single ep, *__single match;
	int prio, matchprio;
	void *__single match_arg = NULL;

	/* Expect 32-bit aligned data pointer on strict-align platforms */
	MBUF_STRICT_DATA_ALIGNMENT_CHECK_32(m);

	ip6 = mtod(m, struct ip6_hdr *);
	SOCKADDR_ZERO(&s, sizeof(s));
	s.sin6_family = AF_INET6;
	s.sin6_len = sizeof(struct sockaddr_in6);
	s.sin6_addr = ip6->ip6_src;
	SOCKADDR_ZERO(&d, sizeof(d));
	d.sin6_family = AF_INET6;
	d.sin6_len = sizeof(struct sockaddr_in6);
	d.sin6_addr = ip6->ip6_dst;

	match = NULL;
	matchprio = 0;

	lck_rw_lock_shared(&encaptab_lock);
	for (ep = LIST_FIRST(&encaptab); ep; ep = LIST_NEXT(ep, chain)) {
		if (ep->af != AF_INET6) {
			continue;
		}
		if (ep->proto >= 0 && ep->proto != proto) {
			continue;
		}
		if (ep->func) {
			prio = (*ep->func)(m, *offp, proto, ep->arg);
		} else {
			/*
			 * it's inbound traffic, we need to match in reverse
			 * order
			 */
			prio = mask_match(ep, SA(&d), SA(&s));
		}

		/* see encap4_input() for issues here */
		if (prio <= 0) {
			continue;
		}
		if (prio > matchprio) {
			matchprio = prio;
			match = ep;
			psw = (const struct ip6protosw *)match->psw;
			match_arg = ep->arg;
		}
	}
	lck_rw_unlock_shared(&encaptab_lock);

	if (match) {
		/* found a match */
		if (psw && psw->pr_input) {
			encap_fillarg(m, match_arg);
			return (*psw->pr_input)(mp, offp, proto);
		} else {
			m_freem(m);
			return IPPROTO_DONE;
		}
	}

	/* last resort: inject to raw socket */
	return rip6_input(mp, offp, proto);
}

static void
encap_add_locked(struct encaptab *ep)
{
	LCK_RW_ASSERT(&encaptab_lock, LCK_RW_ASSERT_EXCLUSIVE);
	LIST_INSERT_HEAD(&encaptab, ep, chain);
}

/*
 * sp (src ptr) is always my side, and dp (dst ptr) is always remote side.
 * length of mask (sm and dm) is assumed to be same as sp/dp.
 * Return value will be necessary as input (cookie) for encap_detach().
 */
const struct encaptab *
encap_attach(int af, int proto, const struct sockaddr *sp,
    const struct sockaddr *sm, const struct sockaddr *dp,
    const struct sockaddr *dm, const struct protosw *psw, void *arg)
{
	struct encaptab *ep = NULL;
	struct encaptab *new_ep = NULL;
	int error;

	/* sanity check on args */
	if (sp->sa_len > sizeof(new_ep->src) || dp->sa_len > sizeof(new_ep->dst)) {
		error = EINVAL;
		goto fail;
	}
	if (sp->sa_len != dp->sa_len) {
		error = EINVAL;
		goto fail;
	}
	if (af != sp->sa_family || af != dp->sa_family) {
		error = EINVAL;
		goto fail;
	}

	new_ep = kalloc_type(struct encaptab, Z_WAITOK | Z_ZERO | Z_NOFAIL);

	/* check if anyone have already attached with exactly same config */
	lck_rw_lock_exclusive(&encaptab_lock);
	for (ep = LIST_FIRST(&encaptab); ep; ep = LIST_NEXT(ep, chain)) {
		if (ep->af != af) {
			continue;
		}
		if (ep->proto != proto) {
			continue;
		}
		if (ep->src.ss_len != sp->sa_len ||
		    SOCKADDR_CMP(&ep->src, sp, sp->sa_len) != 0 ||
		    SOCKADDR_CMP(&ep->srcmask, sm, sp->sa_len) != 0) {
			continue;
		}
		if (ep->dst.ss_len != dp->sa_len ||
		    SOCKADDR_CMP(&ep->dst, dp, dp->sa_len) != 0 ||
		    SOCKADDR_CMP(&ep->dstmask, dm, dp->sa_len) != 0) {
			continue;
		}

		error = EEXIST;
		goto fail_locked;
	}

	new_ep->af = af;
	new_ep->proto = proto;
	SOCKADDR_COPY(sp, &new_ep->src, sp->sa_len);
	SOCKADDR_COPY(sm, &new_ep->srcmask, sp->sa_len);
	SOCKADDR_COPY(dp, &new_ep->dst, dp->sa_len);
	SOCKADDR_COPY(dm, &new_ep->dstmask, dp->sa_len);
	new_ep->psw = psw;
	new_ep->arg = arg;

	encap_add_locked(new_ep);
	lck_rw_unlock_exclusive(&encaptab_lock);

	error = 0;
	return new_ep;

fail_locked:
	lck_rw_unlock_exclusive(&encaptab_lock);
	if (new_ep != NULL) {
		kfree_type(struct encaptab, new_ep);
	}
fail:
	return NULL;
}

const struct encaptab *
encap_attach_func( int af, int proto,
    int (*func)(const struct mbuf *, int, int, void *),
    const struct protosw *psw, void *arg)
{
	struct encaptab *ep;
	int error;

	/* sanity check on args */
	if (!func) {
		error = EINVAL;
		goto fail;
	}

	ep = kalloc_type(struct encaptab, Z_WAITOK | Z_ZERO | Z_NOFAIL); /* XXX */

	ep->af = af;
	ep->proto = proto;
	ep->func = func;
	ep->psw = psw;
	ep->arg = arg;

	lck_rw_lock_exclusive(&encaptab_lock);
	encap_add_locked(ep);
	lck_rw_unlock_exclusive(&encaptab_lock);

	error = 0;
	return ep;

fail:
	return NULL;
}

int
encap_detach(const struct encaptab *cookie)
{
	const struct encaptab *ep = cookie;
	struct encaptab *p;

	lck_rw_lock_exclusive(&encaptab_lock);
	for (p = LIST_FIRST(&encaptab); p; p = LIST_NEXT(p, chain)) {
		if (p == ep) {
			LIST_REMOVE(p, chain);
			lck_rw_unlock_exclusive(&encaptab_lock);
			kfree_type(struct encaptab, p);    /*XXX*/
			return 0;
		}
	}
	lck_rw_unlock_exclusive(&encaptab_lock);

	return EINVAL;
}

static int
mask_match(const struct encaptab *ep, const struct sockaddr *sp,
    const struct sockaddr *dp)
{
	struct sockaddr_storage s;
	struct sockaddr_storage d;
	int i;
	const u_int8_t *p, *q;
	u_int8_t *r;
	int matchlen;

	if (sp->sa_len > sizeof(s) || dp->sa_len > sizeof(d)) {
		return 0;
	}
	if (sp->sa_family != ep->af || dp->sa_family != ep->af) {
		return 0;
	}
	if (sp->sa_len != ep->src.ss_len || dp->sa_len != ep->dst.ss_len) {
		return 0;
	}

	matchlen = 0;

	p = SA_BYTES(sp);
	q = SA_BYTES(&ep->srcmask);
	r = SA_BYTES(&s);
	for (i = 0; i < sp->sa_len; i++) {
		r[i] = p[i] & q[i];
		/* XXX estimate */
		matchlen += (q[i] ? 8 : 0);
	}

	p = SA_BYTES(dp);
	q = SA_BYTES(&ep->dstmask);
	r = SA_BYTES(&s);
	for (i = 0; i < dp->sa_len; i++) {
		r[i] = p[i] & q[i];
		/* XXX rough estimate */
		matchlen += (q[i] ? 8 : 0);
	}

	/* need to overwrite len/family portion as we don't compare them */
	s.ss_len = sp->sa_len;
	s.ss_family = sp->sa_family;
	d.ss_len = dp->sa_len;
	d.ss_family = dp->sa_family;

	if (bcmp(&s, &ep->src, ep->src.ss_len) == 0 &&
	    bcmp(&d, &ep->dst, ep->dst.ss_len) == 0) {
		return matchlen;
	} else {
		return 0;
	}
}

struct encaptabtag {
	void*                   *arg;
};

static void
encap_fillarg(
	struct mbuf *m,
	void *arg)
{
	struct m_tag    *tag;
	struct encaptabtag *et;

	tag = m_tag_create(KERNEL_MODULE_TAG_ID, KERNEL_TAG_TYPE_ENCAP,
	    sizeof(struct encaptabtag), M_WAITOK, m);

	if (tag != NULL) {
		et = (struct encaptabtag*)(tag->m_tag_data);
		et->arg = arg;
		m_tag_prepend(m, tag);
	}
}

void *
encap_getarg(struct mbuf *m)
{
	struct m_tag *__single tag;
	struct encaptabtag *__single et;
	void *__single p = NULL;

	tag = m_tag_locate(m, KERNEL_MODULE_TAG_ID, KERNEL_TAG_TYPE_ENCAP);
	if (tag) {
		et = (struct encaptabtag*)(tag->m_tag_data);
		p = et->arg;
		m_tag_delete(m, tag);
	}

	return p;
}

struct encaptab_tag_container {
	struct m_tag            encaptab_m_tag;
	struct encaptabtag      encaptab_tag;
};

static struct m_tag *
m_tag_kalloc_encap(u_int32_t id, u_int16_t type, uint16_t len, int wait)
{
	struct encaptab_tag_container *tag_container;
	struct m_tag *tag = NULL;

	assert3u(id, ==, KERNEL_MODULE_TAG_ID);
	assert3u(type, ==, KERNEL_TAG_TYPE_ENCAP);
	assert3u(len, ==, sizeof(struct encaptabtag));

	if (len != sizeof(struct encaptabtag)) {
		return NULL;
	}

	tag_container = kalloc_type(struct encaptab_tag_container, wait | M_ZERO);
	if (tag_container != NULL) {
		tag = &tag_container->encaptab_m_tag;

		assert3p(tag, ==, tag_container);

		M_TAG_INIT(tag, id, type, len, &tag_container->encaptab_tag, NULL);
	}

	return tag;
}

static void
m_tag_kfree_encap(struct m_tag *tag)
{
	struct encaptab_tag_container *__single tag_container = (struct encaptab_tag_container *)tag;

	assert3u(tag->m_tag_len, ==, sizeof(struct encaptabtag));

	kfree_type(struct encaptab_tag_container, tag_container);
}

void
encap_register_m_tag(void)
{
	int error;

	error = m_register_internal_tag_type(KERNEL_TAG_TYPE_ENCAP, sizeof(struct encaptabtag),
	    m_tag_kalloc_encap, m_tag_kfree_encap);

	assert3u(error, ==, 0);
}
