/*
 * Copyright (c) 2007-2021 Apple Inc. All rights reserved.
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

/*	$apfw: pf_if.c,v 1.4 2008/08/27 00:01:32 jhw Exp $ */
/*	$OpenBSD: pf_if.c,v 1.46 2006/12/13 09:01:59 itojun Exp $ */

/*
 * Copyright 2005 Henning Brauer <henning@openbsd.org>
 * Copyright 2005 Ryan McBride <mcbride@openbsd.org>
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2003 Cedric Berger
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/filio.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <netinet/ip6.h>

#include <net/pfvar.h>

struct pfi_kif                  *pfi_all = NULL;

static struct pool              pfi_addr_pl;
static struct pfi_ifhead        pfi_ifs;
static u_int32_t                pfi_update = 1;
static int                      pfi_buffer_max;
static struct pfr_addr          *__counted_by(pfi_buffer_max) pfi_buffer;
static int                      pfi_buffer_cnt;

__private_extern__ void pfi_kifaddr_update(void *);

static void pfi_kif_update(struct pfi_kif *);
static void pfi_dynaddr_update(struct pfi_dynaddr *dyn);
static void pfi_table_update(struct pfr_ktable *, struct pfi_kif *, uint8_t, int);
static void pfi_instance_add(struct ifnet *, uint8_t, int);
static void pfi_address_add(struct sockaddr *, uint8_t, uint8_t);
static int pfi_if_compare(struct pfi_kif *, struct pfi_kif *);
static int pfi_skip_if(const char *, struct pfi_kif *);
static uint8_t pfi_unmask(void *);

RB_PROTOTYPE_SC(static, pfi_ifhead, pfi_kif, pfik_tree, pfi_if_compare);
RB_GENERATE(pfi_ifhead, pfi_kif, pfik_tree, pfi_if_compare);

#define PFI_BUFFER_MAX          0x10000

#define IFG_ALL "ALL"

void
pfi_initialize(void)
{
	if (pfi_all != NULL) {  /* already initialized */
		return;
	}

	pool_init(&pfi_addr_pl, sizeof(struct pfi_dynaddr), 0, 0, 0,
	    "pfiaddrpl", NULL);
	pfi_buffer = (struct pfr_addr *)kalloc_data(64 * sizeof(*pfi_buffer),
	    Z_WAITOK);
	pfi_buffer_max = 64;

	if ((pfi_all = pfi_kif_get(IFG_ALL)) == NULL) {
		panic("pfi_kif_get for pfi_all failed");
	}
}

#if 0
void
pfi_destroy(void)
{
	pool_destroy(&pfi_addr_pl);
	kfree_data(pfi_buffer, pfi_buffer_max * sizeof(*pfi_buffer));
}
#endif

struct pfi_kif *
pfi_kif_get(const char *kif_name)
{
	struct pfi_kif          *__single kif;
	struct pfi_kif       s;

	bzero(&s.pfik_name, sizeof(s.pfik_name));
	strlcpy(s.pfik_name, kif_name, sizeof(s.pfik_name));
	kif = RB_FIND(pfi_ifhead, &pfi_ifs, &s);
	if (kif != NULL) {
		return kif;
	}

	/* create new one */
	if ((kif = kalloc_type(struct pfi_kif, Z_WAITOK | Z_ZERO)) == NULL) {
		return NULL;
	}

	strlcpy(kif->pfik_name, kif_name, sizeof(kif->pfik_name));
	kif->pfik_tzero = pf_calendar_time_second();
	TAILQ_INIT(&kif->pfik_dynaddrs);

	RB_INSERT(pfi_ifhead, &pfi_ifs, kif);
	return kif;
}

void
pfi_kif_ref(struct pfi_kif *kif, enum pfi_kif_refs what)
{
	switch (what) {
	case PFI_KIF_REF_RULE:
		kif->pfik_rules++;
		break;
	case PFI_KIF_REF_STATE:
		kif->pfik_states++;
		break;
	default:
		panic("pfi_kif_ref with unknown type");
	}
}

void
pfi_kif_unref(struct pfi_kif *kif, enum pfi_kif_refs what)
{
	if (kif == NULL) {
		return;
	}

	switch (what) {
	case PFI_KIF_REF_NONE:
		break;
	case PFI_KIF_REF_RULE:
		if (kif->pfik_rules <= 0) {
			printf("pfi_kif_unref: rules refcount <= 0\n");
			return;
		}
		kif->pfik_rules--;
		break;
	case PFI_KIF_REF_STATE:
		if (kif->pfik_states <= 0) {
			printf("pfi_kif_unref: state refcount <= 0\n");
			return;
		}
		kif->pfik_states--;
		break;
	default:
		panic("pfi_kif_unref with unknown type");
	}

	if (kif->pfik_ifp != NULL || kif == pfi_all) {
		return;
	}

	if (kif->pfik_rules || kif->pfik_states) {
		return;
	}

	RB_REMOVE(pfi_ifhead, &pfi_ifs, kif);
	kfree_type(struct pfi_kif, kif);
}

int
pfi_kif_match(struct pfi_kif *rule_kif, struct pfi_kif *packet_kif)
{
	if (rule_kif == NULL || rule_kif == packet_kif) {
		return 1;
	}

	return 0;
}

void
pfi_attach_ifnet(struct ifnet *ifp)
{
	struct pfi_kif *kif;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	pfi_update++;
	if ((kif = pfi_kif_get(if_name(ifp))) == NULL) {
		panic("pfi_kif_get failed");
	}

	ifnet_lock_exclusive(ifp);
	kif->pfik_ifp = ifp;
	ifp->if_pf_kif = kif;
	ifnet_lock_done(ifp);

	pfi_kif_update(kif);
}

/*
 * Caller holds ifnet lock as writer (exclusive);
 */
void
pfi_detach_ifnet(struct ifnet *ifp)
{
	struct pfi_kif          *kif;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	if ((kif = (struct pfi_kif *)ifp->if_pf_kif) == NULL) {
		return;
	}

	pfi_update++;
	pfi_kif_update(kif);

	ifnet_lock_exclusive(ifp);
	kif->pfik_ifp = NULL;
	ifp->if_pf_kif = NULL;
	ifnet_lock_done(ifp);

	pfi_kif_unref(kif, PFI_KIF_REF_NONE);
}

int
pfi_match_addr(struct pfi_dynaddr *dyn, struct pf_addr *a, sa_family_t af)
{
	switch (af) {
#if INET
	case AF_INET:
		switch (dyn->pfid_acnt4) {
		case 0:
			return 0;
		case 1:
			return PF_MATCHA(0, &dyn->pfid_addr4,
			           &dyn->pfid_mask4, a, AF_INET);
		default:
			return pfr_match_addr(dyn->pfid_kt, a, AF_INET);
		}
#endif /* INET */
	case AF_INET6:
		switch (dyn->pfid_acnt6) {
		case 0:
			return 0;
		case 1:
			return PF_MATCHA(0, &dyn->pfid_addr6,
			           &dyn->pfid_mask6, a, AF_INET6);
		default:
			return pfr_match_addr(dyn->pfid_kt, a, AF_INET6);
		}
	default:
		return 0;
	}
}

int
pfi_dynaddr_setup(struct pf_addr_wrap *aw, sa_family_t af)
{
	struct pfi_dynaddr      *__single dyn;
	char                     tblnamebuf[PF_TABLE_NAME_SIZE];
	const char              *__null_terminated tblname = NULL;
	struct pf_ruleset       *__single ruleset = NULL;
	int                      rv = 0;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	if (aw->type != PF_ADDR_DYNIFTL) {
		return 0;
	}
	if ((dyn = pool_get(&pfi_addr_pl, PR_WAITOK)) == NULL) {
		return 1;
	}
	bzero(dyn, sizeof(*dyn));

	if (strlcmp(aw->v.ifname, "self", sizeof(aw->v.ifname)) == 0) {
		dyn->pfid_kif = pfi_kif_get(IFG_ALL);
	} else {
		dyn->pfid_kif = pfi_kif_get(__unsafe_null_terminated_from_indexable(aw->v.ifname));
	}
	if (dyn->pfid_kif == NULL) {
		rv = 1;
		goto _bad;
	}
	pfi_kif_ref(dyn->pfid_kif, PFI_KIF_REF_RULE);

	dyn->pfid_net = pfi_unmask(&aw->v.a.mask);
	if (af == AF_INET && dyn->pfid_net == 32) {
		dyn->pfid_net = 128;
	}
	strbufcpy(tblnamebuf, aw->v.ifname);
	if (aw->iflags & PFI_AFLAG_NETWORK) {
		strlcat(tblnamebuf, ":network", sizeof(tblnamebuf));
	}
	if (aw->iflags & PFI_AFLAG_BROADCAST) {
		strlcat(tblnamebuf, ":broadcast", sizeof(tblnamebuf));
	}
	if (aw->iflags & PFI_AFLAG_PEER) {
		strlcat(tblnamebuf, ":peer", sizeof(tblnamebuf));
	}
	if (aw->iflags & PFI_AFLAG_NOALIAS) {
		strlcat(tblnamebuf, ":0", sizeof(tblnamebuf));
	}
	if (dyn->pfid_net == 128) {
		tblname = __unsafe_null_terminated_from_indexable(tblnamebuf);
	} else {
		tblname = tsnprintf(tblnamebuf + strbuflen(tblnamebuf),
		    sizeof(tblnamebuf) - strbuflen(tblnamebuf), "/%d", dyn->pfid_net);
	}
	if ((ruleset = pf_find_or_create_ruleset(PF_RESERVED_ANCHOR)) == NULL) {
		rv = 1;
		goto _bad;
	}

	if ((dyn->pfid_kt = pfr_attach_table(ruleset, tblname)) == NULL) {
		rv = 1;
		goto _bad;
	}

	dyn->pfid_kt->pfrkt_flags |= PFR_TFLAG_ACTIVE;
	dyn->pfid_iflags = aw->iflags;
	dyn->pfid_af = af;

	TAILQ_INSERT_TAIL(&dyn->pfid_kif->pfik_dynaddrs, dyn, entry);
	aw->p.dyn = dyn;
	pfi_kif_update(dyn->pfid_kif);
	return 0;

_bad:
	if (dyn->pfid_kt != NULL) {
		pfr_detach_table(dyn->pfid_kt);
	}
	if (ruleset != NULL) {
		pf_release_ruleset(ruleset);
	}
	if (dyn->pfid_kif != NULL) {
		pfi_kif_unref(dyn->pfid_kif, PFI_KIF_REF_RULE);
	}
	pool_put(&pfi_addr_pl, dyn);
	return rv;
}

void
pfi_kif_update(struct pfi_kif *kif)
{
	struct pfi_dynaddr      *p;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	/* update all dynaddr */
	TAILQ_FOREACH(p, &kif->pfik_dynaddrs, entry)
	pfi_dynaddr_update(p);
}

void
pfi_dynaddr_update(struct pfi_dynaddr *dyn)
{
	struct pfi_kif          *kif;
	struct pfr_ktable       *kt;

	if (dyn == NULL || dyn->pfid_kif == NULL || dyn->pfid_kt == NULL) {
		panic("pfi_dynaddr_update");
	}

	kif = dyn->pfid_kif;
	kt = dyn->pfid_kt;

	if (kt->pfrkt_larg != pfi_update) {
		/* this table needs to be brought up-to-date */
		pfi_table_update(kt, kif, dyn->pfid_net, dyn->pfid_iflags);
		kt->pfrkt_larg = pfi_update;
	}
	pfr_dynaddr_update(kt, dyn);
}

void
pfi_table_update(struct pfr_ktable *kt, struct pfi_kif *kif, uint8_t net, int flags)
{
	int                      e, size2 = 0;

	pfi_buffer_cnt = 0;

	if (kif->pfik_ifp != NULL) {
		pfi_instance_add(kif->pfik_ifp, net, flags);
	}

	if ((e = pfr_set_addrs(&kt->pfrkt_t, CAST_USER_ADDR_T(pfi_buffer),
	    pfi_buffer_cnt, &size2, NULL, NULL, NULL, 0, PFR_TFLAG_ALLMASK))) {
		printf("pfi_table_update: cannot set %d new addresses "
		    "into table %s: %d\n", pfi_buffer_cnt, kt->pfrkt_name, e);
	}
}

void
pfi_instance_add(struct ifnet *ifp, uint8_t net, int flags)
{
	struct ifaddr   *__single ia;
	int              got4 = 0, got6 = 0;
	uint8_t          net2, af;

	if (ifp == NULL) {
		return;
	}
	ifnet_lock_shared(ifp);
	TAILQ_FOREACH(ia, &ifp->if_addrhead, ifa_link) {
		IFA_LOCK(ia);
		if (ia->ifa_addr == NULL) {
			IFA_UNLOCK(ia);
			continue;
		}
		af = ia->ifa_addr->sa_family;
		if (af != AF_INET && af != AF_INET6) {
			IFA_UNLOCK(ia);
			continue;
		}
		if ((flags & PFI_AFLAG_BROADCAST) && af == AF_INET6) {
			IFA_UNLOCK(ia);
			continue;
		}
		if ((flags & PFI_AFLAG_BROADCAST) &&
		    !(ifp->if_flags & IFF_BROADCAST)) {
			IFA_UNLOCK(ia);
			continue;
		}
		if ((flags & PFI_AFLAG_PEER) &&
		    !(ifp->if_flags & IFF_POINTOPOINT)) {
			IFA_UNLOCK(ia);
			continue;
		}
		if ((af == AF_INET6) &&
		    IN6_IS_ADDR_LINKLOCAL(&((struct sockaddr_in6 *)
		    (void *)ia->ifa_addr)->sin6_addr)) {
			IFA_UNLOCK(ia);
			continue;
		}
		if ((af == AF_INET6) &&
		    ((ifatoia6(ia))->ia6_flags &
		    (IN6_IFF_ANYCAST | IN6_IFF_NOTREADY | IN6_IFF_DETACHED |
		    IN6_IFF_CLAT46 | IN6_IFF_TEMPORARY | IN6_IFF_DEPRECATED))) {
			IFA_UNLOCK(ia);
			continue;
		}
		if (flags & PFI_AFLAG_NOALIAS) {
			if (af == AF_INET && got4) {
				IFA_UNLOCK(ia);
				continue;
			}
			if (af == AF_INET6 && got6) {
				IFA_UNLOCK(ia);
				continue;
			}
		}
		if (af == AF_INET) {
			got4 = 1;
		} else if (af == AF_INET6) {
			got6 = 1;
		}
		net2 = net;
		if (net2 == 128 && (flags & PFI_AFLAG_NETWORK)) {
			if (af == AF_INET) {
				net2 = pfi_unmask(&((struct sockaddr_in *)
				    (void *)ia->ifa_netmask)->sin_addr);
			} else if (af == AF_INET6) {
				net2 = pfi_unmask(&((struct sockaddr_in6 *)
				    (void *)ia->ifa_netmask)->sin6_addr);
			}
		}
		if (af == AF_INET && net2 > 32) {
			net2 = 32;
		}
		if (flags & PFI_AFLAG_BROADCAST) {
			pfi_address_add(ia->ifa_broadaddr, af, net2);
		} else if (flags & PFI_AFLAG_PEER) {
			pfi_address_add(ia->ifa_dstaddr, af, net2);
		} else {
			pfi_address_add(ia->ifa_addr, af, net2);
		}
		IFA_UNLOCK(ia);
	}
	ifnet_lock_done(ifp);
}

void
pfi_address_add(struct sockaddr *sa, uint8_t af, uint8_t net)
{
	struct pfr_addr *p;
	int              i;

	if (pfi_buffer_cnt >= pfi_buffer_max) {
		int              new_max = pfi_buffer_max * 2;

		if (new_max > PFI_BUFFER_MAX) {
			printf("pfi_address_add: address buffer full (%d/%d)\n",
			    pfi_buffer_cnt, PFI_BUFFER_MAX);
			return;
		}
		p = (struct pfr_addr *)kalloc_data(new_max * sizeof(*pfi_buffer),
		    Z_WAITOK);
		if (p == NULL) {
			printf("pfi_address_add: no memory to grow buffer "
			    "(%d/%d)\n", pfi_buffer_cnt, PFI_BUFFER_MAX);
			return;
		}
		memcpy(p, pfi_buffer, pfi_buffer_max * sizeof(*pfi_buffer));
		/* no need to zero buffer */
		kfree_data_counted_by(pfi_buffer, pfi_buffer_max);
		pfi_buffer = p;
		pfi_buffer_max = new_max;
	}
	if (af == AF_INET && net > 32) {
		net = 128;
	}
	p = pfi_buffer + pfi_buffer_cnt++;
	bzero(p, sizeof(*p));
	p->pfra_af = af;
	p->pfra_net = net;
	if (af == AF_INET) {
		p->pfra_ip4addr = ((struct sockaddr_in *)(void *)sa)->sin_addr;
	} else if (af == AF_INET6) {
		p->pfra_ip6addr =
		    ((struct sockaddr_in6 *)(void *)sa)->sin6_addr;
		if (IN6_IS_SCOPE_EMBED(&p->pfra_ip6addr)) {
			p->pfra_ip6addr.s6_addr16[1] = 0;
		}
	}
	/* mask network address bits */
	if (net < 128) {
		((caddr_t)p)[p->pfra_net / 8] &= ~(0xFF >> (p->pfra_net % 8));
	}
	for (i = (p->pfra_net + 7) / 8; i < (int)sizeof(p->pfra_u); i++) {
		((caddr_t)p)[i] = 0;
	}
}

void
pfi_dynaddr_remove(struct pf_addr_wrap *aw)
{
	if (aw->type != PF_ADDR_DYNIFTL || aw->p.dyn == NULL ||
	    aw->p.dyn->pfid_kif == NULL || aw->p.dyn->pfid_kt == NULL) {
		return;
	}

	TAILQ_REMOVE(&aw->p.dyn->pfid_kif->pfik_dynaddrs, aw->p.dyn, entry);
	pfi_kif_unref(aw->p.dyn->pfid_kif, PFI_KIF_REF_RULE);
	aw->p.dyn->pfid_kif = NULL;
	pfr_detach_table(aw->p.dyn->pfid_kt);
	aw->p.dyn->pfid_kt = NULL;
	pool_put(&pfi_addr_pl, aw->p.dyn);
	aw->p.dyn = NULL;
}

void
pfi_dynaddr_copyout(struct pf_addr_wrap *aw)
{
	if (aw->type != PF_ADDR_DYNIFTL || aw->p.dyn == NULL ||
	    aw->p.dyn->pfid_kif == NULL) {
		return;
	}
	aw->p.dyncnt = aw->p.dyn->pfid_acnt4 + aw->p.dyn->pfid_acnt6;
}

void
pfi_kifaddr_update(void *v)
{
	struct pfi_kif          *kif = (struct pfi_kif *)v;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	pfi_update++;
	pfi_kif_update(kif);
}

int
pfi_if_compare(struct pfi_kif *p, struct pfi_kif *q)
{
	return strbufcmp(p->pfik_name, q->pfik_name);
}

void
pfi_update_status(const char *__null_terminated name, struct pf_status *pfs)
{
	struct pfi_kif          *p;
	struct pfi_kif       key;
	int                      i, j, k;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	bzero(&key.pfik_name, sizeof(key.pfik_name));
	strlcpy(key.pfik_name, name, sizeof(key.pfik_name));
	p = RB_FIND(pfi_ifhead, &pfi_ifs, &key);
	if (p == NULL) {
		return;
	}

	if (pfs != NULL) {
		bzero(pfs->pcounters, sizeof(pfs->pcounters));
		bzero(pfs->bcounters, sizeof(pfs->bcounters));
		for (i = 0; i < 2; i++) {
			for (j = 0; j < 2; j++) {
				for (k = 0; k < 2; k++) {
					pfs->pcounters[i][j][k] +=
					    p->pfik_packets[i][j][k];
					pfs->bcounters[i][j] +=
					    p->pfik_bytes[i][j][k];
				}
			}
		}
	} else {
		/* just clear statistics */
		bzero(p->pfik_packets, sizeof(p->pfik_packets));
		bzero(p->pfik_bytes, sizeof(p->pfik_bytes));
		p->pfik_tzero = pf_calendar_time_second();
	}
}

int
pfi_get_ifaces(const char *name, user_addr_t buf, int *size)
{
	struct pfi_kif   *__single p, *__single nextp;
	int              n = 0;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	for (p = RB_MIN(pfi_ifhead, &pfi_ifs); p; p = nextp) {
		nextp = RB_NEXT(pfi_ifhead, &pfi_ifs, p);
		if (pfi_skip_if(name, p)) {
			continue;
		}
		if (*size > n++) {
			struct pfi_uif u;

			if (!p->pfik_tzero) {
				p->pfik_tzero = pf_calendar_time_second();
			}
			pfi_kif_ref(p, PFI_KIF_REF_RULE);

			/* return the user space version of pfi_kif */
			bzero(&u, sizeof(u));
			bcopy(p->pfik_name, &u.pfik_name, sizeof(u.pfik_name));
			bcopy(p->pfik_packets, &u.pfik_packets,
			    sizeof(u.pfik_packets));
			bcopy(p->pfik_bytes, &u.pfik_bytes,
			    sizeof(u.pfik_bytes));
			u.pfik_tzero = p->pfik_tzero;
			u.pfik_flags = p->pfik_flags;
			u.pfik_states = p->pfik_states;
			u.pfik_rules = p->pfik_rules;

			if (copyout(&u, buf, sizeof(u))) {
				pfi_kif_unref(p, PFI_KIF_REF_RULE);
				return EFAULT;
			}
			buf += sizeof(u);
			nextp = RB_NEXT(pfi_ifhead, &pfi_ifs, p);
			pfi_kif_unref(p, PFI_KIF_REF_RULE);
		}
	}
	*size = n;
	return 0;
}

int
pfi_skip_if(const char *filter, struct pfi_kif *p)
{
	size_t     n;

	if (filter == NULL || !*filter) {
		return 0;
	}
	if (strlcmp(p->pfik_name, filter, sizeof(p->pfik_name)) == 0) {
		return 0;     /* exact match */
	}
	n = strlen(filter);
	if (n < 1 || n >= IFNAMSIZ) {
		return 1;     /* sanity check */
	}
	char const * fp = __null_terminated_to_indexable(filter);
	if (fp[n - 1] >= '0' && fp[n - 1] <= '9') {
		return 1;     /* only do exact match in that case */
	}
	if (strlcmp(p->pfik_name, filter, sizeof(p->pfik_name))) {
		return 1;     /* prefix doesn't match */
	}
	return p->pfik_name[n] < '0' || p->pfik_name[n] > '9';
}

int
pfi_set_flags(const char *name, int flags)
{
	struct pfi_kif  *p;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	RB_FOREACH(p, pfi_ifhead, &pfi_ifs) {
		if (pfi_skip_if(name, p)) {
			continue;
		}
		p->pfik_flags |= flags;
	}
	return 0;
}

int
pfi_clear_flags(const char *name, int flags)
{
	struct pfi_kif  *p;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	RB_FOREACH(p, pfi_ifhead, &pfi_ifs) {
		if (pfi_skip_if(name, p)) {
			continue;
		}
		p->pfik_flags &= ~flags;
	}
	return 0;
}

/* from pf_print_state.c */
uint8_t
pfi_unmask(void *addr)
{
	struct pf_addr *__single m = addr;
	int i = 31, j = 0, b = 0;
	u_int32_t tmp;

	while (j < 4 && m->addr32[j] == 0xffffffff) {
		b += 32;
		j++;
	}
	if (j < 4) {
		tmp = ntohl(m->addr32[j]);
		for (i = 31; tmp & (1 << i); --i) {
			b++;
		}
	}
	VERIFY(b >= 0 && b <= UINT8_MAX);
	return (uint8_t)b;
}
