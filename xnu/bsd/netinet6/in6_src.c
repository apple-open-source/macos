/*
 * Copyright (c) 2000-2022 Apple Inc. All rights reserved.
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
 *	@(#)in_pcb.c	8.2 (Berkeley) 1/4/94
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>
#include <sys/priv.h>
#include <kern/locks.h>
#include <sys/random.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/restricted_in_port.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>

#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet6/nd6.h>

#include <net/net_osdep.h>

#include <net/sockaddr_utils.h>

#include "loop.h"

SYSCTL_DECL(_net_inet6_ip6);

static int ip6_select_srcif_debug = 0;
SYSCTL_INT(_net_inet6_ip6, OID_AUTO, select_srcif_debug,
    CTLFLAG_RW | CTLFLAG_LOCKED, &ip6_select_srcif_debug, 0,
    "log source interface selection debug info");

static int ip6_select_srcaddr_debug = 0;
SYSCTL_INT(_net_inet6_ip6, OID_AUTO, select_srcaddr_debug,
    CTLFLAG_RW | CTLFLAG_LOCKED, &ip6_select_srcaddr_debug, 0,
    "log source address selection debug info");

static int ip6_select_src_expensive_secondary_if = 0;
SYSCTL_INT(_net_inet6_ip6, OID_AUTO, select_src_expensive_secondary_if,
    CTLFLAG_RW | CTLFLAG_LOCKED, &ip6_select_src_expensive_secondary_if, 0,
    "allow source interface selection to use expensive secondaries");

static int ip6_select_src_strong_end = 1;
SYSCTL_INT(_net_inet6_ip6, OID_AUTO, select_src_strong_end,
    CTLFLAG_RW | CTLFLAG_LOCKED, &ip6_select_src_strong_end, 0,
    "limit source address selection to outgoing interface");

#define ADDR_LABEL_NOTAPP (-1)
struct in6_addrpolicy defaultaddrpolicy;

int ip6_prefer_tempaddr = 1;

int ip6_cga_conflict_retries = IPV6_CGA_CONFLICT_RETRIES_DEFAULT;

extern int      udp_use_randomport;
extern int      tcp_use_randomport;

static int selectroute(struct sockaddr_in6 *, struct sockaddr_in6 *,
    struct ip6_pktopts *, struct ip6_moptions *, struct in6_ifaddr **,
    struct route_in6 *, struct ifnet **, struct rtentry **, int, int,
    struct ip6_out_args *ip6oa);
static int in6_selectif(struct sockaddr_in6 *, struct ip6_pktopts *,
    struct ip6_moptions *, struct route_in6 *ro,
    struct ip6_out_args *, struct ifnet **);
static void init_policy_queue(void);
static int add_addrsel_policyent(const struct in6_addrpolicy *);
static int walk_addrsel_policy(int (*)(const struct in6_addrpolicy *, void *),
    void *);
static int dump_addrsel_policyent(const struct in6_addrpolicy *, void *);
static struct in6_addrpolicy *match_addrsel_policy(struct sockaddr_in6 *);
void addrsel_policy_init(void);

#define SASEL_DO_DBG(inp) \
	(ip6_select_srcaddr_debug && (inp) != NULL && \
	    (inp)->inp_socket != NULL && \
	    ((inp)->inp_socket->so_options & SO_DEBUG))

#define SASEL_LOG(fmt, ...) \
do { \
	if (srcsel_debug) \
	        os_log(OS_LOG_DEFAULT, "%s:%d " fmt,\
	            __FUNCTION__, __LINE__, ##__VA_ARGS__); \
} while (0); \

/*
 * Return an IPv6 address, which is the most appropriate for a given
 * destination and user specified options.
 * If necessary, this function lookups the routing table and returns
 * an entry to the caller for later use.
 */
#define REPLACE(r) do {\
	SASEL_LOG("REPLACE r %s ia %s ifp1 %s\n", \
	    (#r), s_src, ifp1->if_xname); \
	srcrule = (r); \
	goto replace; \
} while (0)

#define NEXTSRC(r) do {\
	SASEL_LOG("NEXTSRC r %s ia %s ifp1 %s\n", \
	    (#r), s_src, ifp1->if_xname); \
	goto next;              /* XXX: we can't use 'continue' here */ \
} while (0)

#define BREAK(r) do { \
	SASEL_LOG("BREAK r %s ia %s ifp1 %s\n", \
	    (#r), s_src, ifp1->if_xname); \
	srcrule = (r); \
	goto out;               /* XXX: we can't use 'break' here */ \
} while (0)


struct ifaddr *
in6_selectsrc_core_ifa(struct sockaddr_in6 *addr, struct ifnet *ifp, int srcsel_debug)
{
	int err = 0;
	struct ifnet *__single src_ifp = NULL;
	struct in6_addr src_storage = {};
	struct in6_addr *__single in6 = NULL;
	struct ifaddr *__single ifa = NULL;

	if ((in6 = in6_selectsrc_core(addr,
	    (ip6_prefer_tempaddr ? IPV6_SRCSEL_HINT_PREFER_TMPADDR : 0),
	    ifp, 0, &src_storage, &src_ifp, &err, &ifa, NULL, FALSE)) == NULL) {
		if (err == 0) {
			err = EADDRNOTAVAIL;
		}
		VERIFY(src_ifp == NULL);
		if (ifa != NULL) {
			ifa_remref(ifa);
			ifa = NULL;
		}
		goto done;
	}

	if (src_ifp != ifp) {
		if (err == 0) {
			err = ENETUNREACH;
		}
		if (ifa != NULL) {
			ifa_remref(ifa);
			ifa = NULL;
		}
		goto done;
	}

	VERIFY(ifa != NULL);
	ifnet_lock_shared(ifp);
	if ((ifa->ifa_debug & IFD_DETACHING) != 0) {
		err = EHOSTUNREACH;
		ifnet_lock_done(ifp);
		ifa_remref(ifa);
		ifa = NULL;
		goto done;
	}
	ifnet_lock_done(ifp);

done:
	SASEL_LOG("Returned with error: %d", err);
	if (src_ifp != NULL) {
		ifnet_release(src_ifp);
	}
	return ifa;
}

struct in6_addr *
in6_selectsrc_core(struct sockaddr_in6 *dstsock, uint32_t hint_mask,
    struct ifnet *ifp, int srcsel_debug, struct in6_addr *src_storage,
    struct ifnet **sifp, int *errorp, struct ifaddr **ifapp, struct route_in6 *ro,
    boolean_t is_for_clat46)
{
	u_int32_t odstzone;
	int bestrule = IP6S_SRCRULE_0;
	struct in6_addrpolicy *__single dst_policy = NULL, *__single best_policy = NULL;
	struct in6_addr dst;
	struct in6_ifaddr *__single ia = NULL, *__single ia_best = NULL;
	char s_src[MAX_IPv6_STR_LEN] = {0};
	char s_dst[MAX_IPv6_STR_LEN] = {0};
	const struct in6_addr *__single  tmp = NULL;
	int dst_scope = -1, best_scope = -1, best_matchlen = -1;
	uint64_t secs = net_uptime();
	struct nd_defrouter *__single dr = NULL;
	uint32_t genid = in6_ifaddrlist_genid;
	VERIFY(dstsock != NULL);
	VERIFY(src_storage != NULL);
	VERIFY(ifp != NULL);

	if (sifp != NULL) {
		*sifp = NULL;
	}

	if (ifapp != NULL) {
		*ifapp = NULL;
	}

	dst = dstsock->sin6_addr; /* make a copy for local operation */

	if (srcsel_debug) {
		(void) inet_ntop(AF_INET6, &dst, s_dst, sizeof(s_src));

		tmp = &in6addr_any;
		(void) inet_ntop(AF_INET6, tmp, s_src, sizeof(s_src));
		os_log(OS_LOG_DEFAULT, "%s out src %s dst %s ifp %s",
		    __func__, s_src, s_dst, ifp->if_xname);
	}

	*errorp = in6_setscope(&dst, ifp, &odstzone);
	if (*errorp != 0) {
		src_storage = NULL;
		goto done;
	}

	/*
	 * Determine if the route is an indirect here
	 * and if it is get the default router that would be
	 * used as next hop.
	 * Later in the function it is used to apply rule 5.5 of RFC 6724.
	 */
	if (ro != NULL && ro->ro_rt != NULL &&
	    (ro->ro_rt->rt_flags & RTF_GATEWAY) &&
	    ro->ro_rt->rt_gateway != NULL) {
		struct rtentry *__single rt = ro->ro_rt;
		lck_mtx_lock(nd6_mutex);
		dr = defrouter_lookup(NULL,
		    &SIN6(rt->rt_gateway)->sin6_addr, rt->rt_ifp);
		lck_mtx_unlock(nd6_mutex);
	}

	lck_rw_lock_shared(&in6_ifaddr_rwlock);
addrloop:
	TAILQ_FOREACH(ia, &in6_ifaddrhead, ia6_link) {
		int new_scope = -1, new_matchlen = -1;
		struct in6_addrpolicy *__single new_policy = NULL;
		u_int32_t srczone = 0, osrczone, dstzone;
		struct in6_addr src;
		struct ifnet *__single ifp1 = ia->ia_ifp;
		int srcrule;

		if (srcsel_debug) {
			(void) inet_ntop(AF_INET6, &ia->ia_addr.sin6_addr,
			    s_src, sizeof(s_src));
		}

		IFA_LOCK(&ia->ia_ifa);

		/*
		 * Simply skip addresses reserved for CLAT46
		 */
		if (!is_for_clat46 && (ia->ia6_flags & IN6_IFF_CLAT46)) {
			SASEL_LOG("NEXT ia %s address on ifp1 %s skipped as it is "
			    "reserved for CLAT46\n", s_src, ifp1->if_xname);
			goto next;
		}

		if (is_for_clat46 && !(ia->ia6_flags & IN6_IFF_CLAT46)) {
			SASEL_LOG("CLAT46: NEXT ia %s address on ifp1 %s skipped as it is "
			    "not reserved for CLAT46\n", s_src, ifp1->if_xname);
			goto next;
		}

		/*
		 * XXX By default we are strong end system and will
		 * limit candidate set of source address to the ones
		 * configured on the outgoing interface.
		 */
		if (ip6_select_src_strong_end &&
		    ifp1 != ifp) {
			SASEL_LOG("NEXT ia %s ifp1 %s address is not on outgoing "
			    "interface \n", s_src, ifp1->if_xname);
			goto next;
		}

		/*
		 * We'll never take an address that breaks the scope zone
		 * of the destination. We also skip an address if its zone
		 * does not contain the outgoing interface.
		 * XXX: we should probably use sin6_scope_id here.
		 */
		if (in6_setscope(&dst, ifp1, &dstzone) ||
		    odstzone != dstzone) {
			SASEL_LOG("NEXT ia %s ifp1 %s odstzone %d != dstzone %d\n",
			    s_src, ifp1->if_xname, odstzone, dstzone);
			goto next;
		}
		src = ia->ia_addr.sin6_addr;
		if (in6_setscope(&src, ifp, &osrczone) ||
		    in6_setscope(&src, ifp1, &srczone) ||
		    osrczone != srczone) {
			SASEL_LOG("NEXT ia %s ifp1 %s osrczone %d != srczone %d\n",
			    s_src, ifp1->if_xname, osrczone, srczone);
			goto next;
		}
		/* avoid unusable addresses */
		if ((ia->ia6_flags &
		    (IN6_IFF_NOTREADY | IN6_IFF_ANYCAST | IN6_IFF_DETACHED))) {
			SASEL_LOG("NEXT ia %s ifp1 %s ia6_flags 0x%x\n",
			    s_src, ifp1->if_xname, ia->ia6_flags);
			goto next;
		}
		if (!ip6_use_deprecated && IFA6_IS_DEPRECATED(ia, secs)) {
			SASEL_LOG("NEXT ia %s ifp1 %s IFA6_IS_DEPRECATED\n",
			    s_src, ifp1->if_xname);
			goto next;
		}
		if (!nd6_optimistic_dad &&
		    (ia->ia6_flags & IN6_IFF_OPTIMISTIC) != 0) {
			SASEL_LOG("NEXT ia %s ifp1 %s IN6_IFF_OPTIMISTIC\n",
			    s_src, ifp1->if_xname);
			goto next;
		}
		/* Rule 1: Prefer same address */
		if (in6_are_addr_equal_scoped(&dst, &ia->ia_addr.sin6_addr, dstzone, srczone)) {
			BREAK(IP6S_SRCRULE_1); /* there should be no better candidate */
		}
		if (ia_best == NULL) {
			REPLACE(IP6S_SRCRULE_0);
		}

		/* Rule 2: Prefer appropriate scope */
		if (dst_scope < 0) {
			dst_scope = in6_addrscope(&dst);
		}
		new_scope = in6_addrscope(&ia->ia_addr.sin6_addr);
		if (IN6_ARE_SCOPE_CMP(best_scope, new_scope) < 0) {
			if (IN6_ARE_SCOPE_CMP(best_scope, dst_scope) < 0) {
				REPLACE(IP6S_SRCRULE_2);
			}
			NEXTSRC(IP6S_SRCRULE_2);
		} else if (IN6_ARE_SCOPE_CMP(new_scope, best_scope) < 0) {
			if (IN6_ARE_SCOPE_CMP(new_scope, dst_scope) < 0) {
				NEXTSRC(IP6S_SRCRULE_2);
			}
			REPLACE(IP6S_SRCRULE_2);
		}

		/*
		 * Rule 3: Avoid deprecated addresses.  Note that the case of
		 * !ip6_use_deprecated is already rejected above.
		 */
		if (!IFA6_IS_DEPRECATED(ia_best, secs) &&
		    IFA6_IS_DEPRECATED(ia, secs)) {
			NEXTSRC(IP6S_SRCRULE_3);
		}
		if (IFA6_IS_DEPRECATED(ia_best, secs) &&
		    !IFA6_IS_DEPRECATED(ia, secs)) {
			REPLACE(IP6S_SRCRULE_3);
		}

		/*
		 * RFC 4429 says that optimistic addresses are equivalent to
		 * deprecated addresses, so avoid them here.
		 */
		if ((ia_best->ia6_flags & IN6_IFF_OPTIMISTIC) == 0 &&
		    (ia->ia6_flags & IN6_IFF_OPTIMISTIC) != 0) {
			NEXTSRC(IP6S_SRCRULE_3);
		}
		if ((ia_best->ia6_flags & IN6_IFF_OPTIMISTIC) != 0 &&
		    (ia->ia6_flags & IN6_IFF_OPTIMISTIC) == 0) {
			REPLACE(IP6S_SRCRULE_3);
		}

		/* Rule 4: Prefer home addresses */
		/*
		 * XXX: This is a TODO.  We should probably merge the MIP6
		 * case above.
		 */

		/* Rule 5: Prefer outgoing interface */
		/*
		 * XXX By default we are strong end with source address
		 * selection. That means all address selection candidate
		 * addresses will be the ones hosted on the outgoing interface
		 * making the following check redundant.
		 */
		if (ip6_select_src_strong_end == 0) {
			if (ia_best->ia_ifp == ifp && ia->ia_ifp != ifp) {
				NEXTSRC(IP6S_SRCRULE_5);
			}
			if (ia_best->ia_ifp != ifp && ia->ia_ifp == ifp) {
				REPLACE(IP6S_SRCRULE_5);
			}
		}

		/*
		 * Rule 5.5: Prefer addresses in a prefix advertised by the next-hop.
		 * If SA or SA's prefix is assigned by the selected next-hop that will
		 * be used to send to D and SB or SB's prefix is assigned by a different
		 * next-hop, then prefer SA.  Similarly, if SB or SB's prefix is
		 * assigned by the next-hop that will be used to send to D and SA or
		 * SA's prefix is assigned by a different next-hop, then prefer SB.
		 */
		if (dr != NULL && ia_best->ia6_ndpr != ia->ia6_ndpr) {
			boolean_t ia_best_has_prefix = FALSE;
			boolean_t ia_has_prefix = FALSE;
			struct nd_prefix ia_best_prefix = {};
			struct nd_prefix ia_prefix = {};
			struct nd_prefix *__single p_ia_best_prefix = NULL;
			struct nd_prefix *__single p_ia_prefix = NULL;

			if (ia_best->ia6_ndpr) {
				ia_best_prefix = *ia_best->ia6_ndpr;
			}

			if (ia->ia6_ndpr) {
				ia_prefix = *ia->ia6_ndpr;
			}

			IFA_UNLOCK(&ia->ia_ifa);
			lck_rw_done(&in6_ifaddr_rwlock);

			p_ia_best_prefix = nd6_prefix_lookup(&ia_best_prefix, ND6_PREFIX_EXPIRY_UNSPEC);
			p_ia_prefix = nd6_prefix_lookup(&ia_prefix, ND6_PREFIX_EXPIRY_UNSPEC);

			lck_mtx_lock(nd6_mutex);
			if (p_ia_best_prefix != NULL) {
				NDPR_LOCK(p_ia_best_prefix);
				ia_best_has_prefix = (pfxrtr_lookup(p_ia_best_prefix, dr) != NULL);
				NDPR_UNLOCK(p_ia_best_prefix);
				NDPR_REMREF(p_ia_best_prefix);
			}
			if (p_ia_prefix != NULL) {
				NDPR_LOCK(p_ia_prefix);
				ia_has_prefix = (pfxrtr_lookup(p_ia_prefix, dr) != NULL);
				NDPR_UNLOCK(p_ia_prefix);
				NDPR_REMREF(p_ia_prefix);
			}
			lck_mtx_unlock(nd6_mutex);

			lck_rw_lock_shared(&in6_ifaddr_rwlock);
			if (genid != os_atomic_load(&in6_ifaddrlist_genid, acquire)) {
				SASEL_LOG("Address list seems to have changed. Restarting source "
				    "address selection.\n");
				genid = in6_ifaddrlist_genid;
				/*
				 * We are starting from scratch. Free up the reference
				 * on ia_best and also reset it to NULL.
				 */
				ifa_remref(&ia_best->ia_ifa);
				ia_best = NULL;
				goto addrloop;
			}
			IFA_LOCK(&ia->ia_ifa);

			if (ia_best_has_prefix && !ia_has_prefix) {
				NEXTSRC(IP6S_SRCRULE_5_5);
			}

			if (!ia_best_has_prefix && ia_has_prefix) {
				REPLACE(IP6S_SRCRULE_5_5);
			}
		}

		/*
		 * Rule 6: Prefer matching label
		 * Note that best_policy should be non-NULL here.
		 */
		if (dst_policy == NULL) {
			dst_policy = in6_addrsel_lookup_policy(dstsock);
		}
		if (dst_policy->label != ADDR_LABEL_NOTAPP) {
			new_policy = in6_addrsel_lookup_policy(&ia->ia_addr);
			if (dst_policy->label == best_policy->label &&
			    dst_policy->label != new_policy->label) {
				NEXTSRC(IP6S_SRCRULE_6);
			}
			if (dst_policy->label != best_policy->label &&
			    dst_policy->label == new_policy->label) {
				REPLACE(IP6S_SRCRULE_6);
			}
		}

		/*
		 * Rule 7: Prefer temporary addresses.
		 * We allow users to reverse the logic by configuring
		 * a sysctl variable, so that transparency conscious users can
		 * always prefer stable addresses.
		 */
		if (!(ia_best->ia6_flags & IN6_IFF_TEMPORARY) &&
		    (ia->ia6_flags & IN6_IFF_TEMPORARY)) {
			if (hint_mask & IPV6_SRCSEL_HINT_PREFER_TMPADDR) {
				REPLACE(IP6S_SRCRULE_7);
			} else {
				NEXTSRC(IP6S_SRCRULE_7);
			}
		}
		if ((ia_best->ia6_flags & IN6_IFF_TEMPORARY) &&
		    !(ia->ia6_flags & IN6_IFF_TEMPORARY)) {
			if (hint_mask & IPV6_SRCSEL_HINT_PREFER_TMPADDR) {
				NEXTSRC(IP6S_SRCRULE_7);
			} else {
				REPLACE(IP6S_SRCRULE_7);
			}
		}

		/*
		 * Rule 7x: prefer addresses on alive interfaces.
		 * This is a KAME specific rule.
		 */
		if ((ia_best->ia_ifp->if_flags & IFF_UP) &&
		    !(ia->ia_ifp->if_flags & IFF_UP)) {
			NEXTSRC(IP6S_SRCRULE_7x);
		}
		if (!(ia_best->ia_ifp->if_flags & IFF_UP) &&
		    (ia->ia_ifp->if_flags & IFF_UP)) {
			REPLACE(IP6S_SRCRULE_7x);
		}

		/*
		 * Rule 8: Use longest matching prefix.
		 */
		new_matchlen = in6_matchlen(&ia->ia_addr.sin6_addr, &dst);
		if (best_matchlen < new_matchlen) {
			REPLACE(IP6S_SRCRULE_8);
		}
		if (new_matchlen < best_matchlen) {
			NEXTSRC(IP6S_SRCRULE_8);
		}

		/*
		 * Last resort: just keep the current candidate.
		 * Or, do we need more rules?
		 */
		if (ifp1 != ifp && (ifp1->if_eflags & IFEF_EXPENSIVE) &&
		    ip6_select_src_expensive_secondary_if == 0) {
			SASEL_LOG("NEXT ia %s ifp1 %s IFEF_EXPENSIVE\n",
			    s_src, ifp1->if_xname);
			ip6stat.ip6s_sources_skip_expensive_secondary_if++;
			goto next;
		}
		SASEL_LOG("NEXT ia %s ifp1 %s last resort\n",
		    s_src, ifp1->if_xname);
		IFA_UNLOCK(&ia->ia_ifa);
		continue;

replace:
		/*
		 * Ignore addresses on secondary interfaces that are marked
		 * expensive
		 */
		if (ifp1 != ifp && (ifp1->if_eflags & IFEF_EXPENSIVE) &&
		    ip6_select_src_expensive_secondary_if == 0) {
			SASEL_LOG("NEXT ia %s ifp1 %s IFEF_EXPENSIVE\n",
			    s_src, ifp1->if_xname);
			ip6stat.ip6s_sources_skip_expensive_secondary_if++;
			goto next;
		}
		bestrule = srcrule;
		best_scope = (new_scope >= 0 ? new_scope :
		    in6_addrscope(&ia->ia_addr.sin6_addr));
		best_policy = (new_policy ? new_policy :
		    in6_addrsel_lookup_policy(&ia->ia_addr));
		best_matchlen = (new_matchlen >= 0 ? new_matchlen :
		    in6_matchlen(&ia->ia_addr.sin6_addr, &dst));
		SASEL_LOG("NEXT ia %s ifp1 %s best_scope %d new_scope %d dst_scope %d\n",
		    s_src, ifp1->if_xname, best_scope, new_scope, dst_scope);
		ifa_addref(&ia->ia_ifa); /* for ia_best */
		IFA_UNLOCK(&ia->ia_ifa);
		if (ia_best != NULL) {
			ifa_remref(&ia_best->ia_ifa);
		}
		ia_best = ia;
		continue;

next:
		IFA_UNLOCK(&ia->ia_ifa);
		continue;

out:
		ifa_addref(&ia->ia_ifa); /* for ia_best */
		IFA_UNLOCK(&ia->ia_ifa);
		if (ia_best != NULL) {
			ifa_remref(&ia_best->ia_ifa);
		}
		ia_best = ia;
		break;
	}

	lck_rw_done(&in6_ifaddr_rwlock);

	if ((ia = ia_best) == NULL) {
		if (*errorp == 0) {
			*errorp = EADDRNOTAVAIL;
		}
		src_storage = NULL;
		goto done;
	}

	if (sifp != NULL) {
		*sifp = ia->ia_ifa.ifa_ifp;
		ifnet_reference(*sifp);
	}

	IFA_LOCK_SPIN(&ia->ia_ifa);
	if (bestrule < IP6S_SRCRULE_COUNT) {
		ip6stat.ip6s_sources_rule[bestrule]++;
	}
	*src_storage = satosin6(&ia->ia_addr)->sin6_addr;
	IFA_UNLOCK(&ia->ia_ifa);

	if (ifapp != NULL) {
		*ifapp = &ia->ia_ifa;
	} else {
		ifa_remref(&ia->ia_ifa);
	}

done:
	if (srcsel_debug) {
		(void) inet_ntop(AF_INET6, &dst, s_dst, sizeof(s_src));

		tmp = (src_storage != NULL) ? src_storage : &in6addr_any;
		(void) inet_ntop(AF_INET6, tmp, s_src, sizeof(s_src));

		os_log(OS_LOG_DEFAULT, "%s out src %s dst %s dst_scope %d best_scope %d",
		    __func__, s_src, s_dst, dst_scope, best_scope);
	}

	if (dr != NULL) {
		NDDR_REMREF(dr);
	}

	return src_storage;
}

/*
 * Regardless of error, it will return an ifp with a reference held if the
 * caller provides a non-NULL ifpp.  The caller is responsible for checking
 * if the returned ifp is valid and release its reference at all times.
 */
struct in6_addr *
in6_selectsrc(struct sockaddr_in6 *dstsock, struct ip6_pktopts *opts,
    struct inpcb *inp, struct route_in6 *ro,
    struct ifnet **ifpp, struct in6_addr *src_storage, unsigned int ifscope,
    int *errorp)
{
	struct ifnet *__single ifp = NULL;
	struct in6_pktinfo *__single pi = NULL;
	struct ip6_moptions *__single mopts;
	struct ip6_out_args ip6oa;
	boolean_t inp_debug = FALSE;
	uint32_t hint_mask = 0;
	int prefer_tempaddr = 0;
	struct ifnet *__single sifp = NULL;

	bzero(&ip6oa, sizeof(ip6oa));
	ip6oa.ip6oa_boundif = ifscope;
	ip6oa.ip6oa_flags = IP6OAF_SELECT_SRCIF;
	ip6oa.ip6oa_sotc = SO_TC_UNSPEC;
	ip6oa.ip6oa_netsvctype = _NET_SERVICE_TYPE_UNSPEC;

	*errorp = 0;
	if (ifpp != NULL) {
		*ifpp = NULL;
	}

	if (inp != NULL) {
		inp_debug = SASEL_DO_DBG(inp);
		mopts = inp->in6p_moptions;
		if (INP_NO_CELLULAR(inp)) {
			ip6oa.ip6oa_flags |= IP6OAF_NO_CELLULAR;
		}
		if (INP_NO_EXPENSIVE(inp)) {
			ip6oa.ip6oa_flags |= IP6OAF_NO_EXPENSIVE;
		}
		if (INP_NO_CONSTRAINED(inp)) {
			ip6oa.ip6oa_flags |= IP6OAF_NO_CONSTRAINED;
		}
		if (INP_AWDL_UNRESTRICTED(inp)) {
			ip6oa.ip6oa_flags |= IP6OAF_AWDL_UNRESTRICTED;
		}
		if (INP_INTCOPROC_ALLOWED(inp)) {
			ip6oa.ip6oa_flags |= IP6OAF_INTCOPROC_ALLOWED;
		}
		if (INP_MANAGEMENT_ALLOWED(inp)) {
			ip6oa.ip6oa_flags |= IP6OAF_MANAGEMENT_ALLOWED;
		}
	} else {
		mopts = NULL;
		/* Allow the kernel to retransmit packets. */
		ip6oa.ip6oa_flags |= IP6OAF_INTCOPROC_ALLOWED |
		    IP6OAF_AWDL_UNRESTRICTED | IP6OAF_MANAGEMENT_ALLOWED;
	}

	if (ip6oa.ip6oa_boundif != IFSCOPE_NONE) {
		ip6oa.ip6oa_flags |= IP6OAF_BOUND_IF;
	}

	/*
	 * If the source address is explicitly specified by the caller,
	 * check if the requested source address is indeed a unicast address
	 * assigned to the node, and can be used as the packet's source
	 * address.  If everything is okay, use the address as source.
	 */
	if (opts && (pi = opts->ip6po_pktinfo) &&
	    !IN6_IS_ADDR_UNSPECIFIED(&pi->ipi6_addr)) {
		struct sockaddr_in6 srcsock;
		struct in6_ifaddr *__single ia6;

		/* get the outgoing interface */
		if ((*errorp = in6_selectif(dstsock, opts, mopts, ro, &ip6oa,
		    &ifp)) != 0) {
			src_storage = NULL;
			goto done;
		}

		/*
		 * determine the appropriate zone id of the source based on
		 * the zone of the destination and the outgoing interface.
		 * If the specified address is ambiguous wrt the scope zone,
		 * the interface must be specified; otherwise, ifa_ifwithaddr()
		 * will fail matching the address.
		 */
		SOCKADDR_ZERO(&srcsock, sizeof(srcsock));
		srcsock.sin6_family = AF_INET6;
		srcsock.sin6_len = sizeof(srcsock);
		srcsock.sin6_addr = pi->ipi6_addr;
		if (ifp != NULL) {
			*errorp = in6_setscope(&srcsock.sin6_addr, ifp, IN6_NULL_IF_EMBEDDED_SCOPE(&srcsock.sin6_scope_id));
			if (*errorp != 0) {
				src_storage = NULL;
				goto done;
			}
		}
		ia6 = ifatoia6(ifa_ifwithaddr(SA(&srcsock)));
		if (ia6 == NULL) {
			*errorp = EADDRNOTAVAIL;
			src_storage = NULL;
			goto done;
		}
		IFA_LOCK_SPIN(&ia6->ia_ifa);
		if ((ia6->ia6_flags & (IN6_IFF_ANYCAST | IN6_IFF_NOTREADY | IN6_IFF_CLAT46)) ||
		    (inp && inp_restricted_send(inp, ia6->ia_ifa.ifa_ifp))) {
			IFA_UNLOCK(&ia6->ia_ifa);
			ifa_remref(&ia6->ia_ifa);
			*errorp = EHOSTUNREACH;
			src_storage = NULL;
			goto done;
		}

		*src_storage = satosin6(&ia6->ia_addr)->sin6_addr;
		IFA_UNLOCK(&ia6->ia_ifa);
		ifa_remref(&ia6->ia_ifa);
		goto done;
	}

	/*
	 * Otherwise, if the socket has already bound the source, just use it.
	 */
	if (inp != NULL && !IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)) {
		src_storage = &inp->in6p_laddr;
		goto done;
	}

	/*
	 * If the address is not specified, choose the best one based on
	 * the outgoing interface and the destination address.
	 */
	/* get the outgoing interface */
	if ((*errorp = in6_selectif(dstsock, opts, mopts, ro, &ip6oa,
	    &ifp)) != 0) {
		src_storage = NULL;
		goto done;
	}

	VERIFY(ifp != NULL);

	if (opts == NULL ||
	    opts->ip6po_prefer_tempaddr == IP6PO_TEMPADDR_SYSTEM) {
		prefer_tempaddr = ip6_prefer_tempaddr;
	} else if (opts->ip6po_prefer_tempaddr == IP6PO_TEMPADDR_NOTPREFER) {
		prefer_tempaddr = 0;
	} else {
		prefer_tempaddr = 1;
	}

	if (prefer_tempaddr) {
		hint_mask |= IPV6_SRCSEL_HINT_PREFER_TMPADDR;
	}

	if (in6_selectsrc_core(dstsock, hint_mask, ifp, inp_debug, src_storage,
	    &sifp, errorp, NULL, ro, FALSE) == NULL) {
		src_storage = NULL;
		goto done;
	}

	VERIFY(sifp != NULL);

	if (inp && inp_restricted_send(inp, sifp)) {
		src_storage = NULL;
		*errorp = EHOSTUNREACH;
		ifnet_release(sifp);
		goto done;
	} else {
		ifnet_release(sifp);
	}

done:
	if (ifpp != NULL) {
		/* if ifp is non-NULL, refcnt held in in6_selectif() */
		*ifpp = ifp;
	} else if (ifp != NULL) {
		ifnet_release(ifp);
	}
	return src_storage;
}

/*
 * Given a source IPv6 address (and route, if available), determine the best
 * interface to send the packet from.  Checking for (and updating) the
 * ROF_SRCIF_SELECTED flag in the pcb-supplied route placeholder is done
 * without any locks, based on the assumption that in the event this is
 * called from ip6_output(), the output operation is single-threaded per-pcb,
 * i.e. for any given pcb there can only be one thread performing output at
 * the IPv6 layer.
 *
 * This routine is analogous to in_selectsrcif() for IPv4.  Regardless of
 * error, it will return an ifp with a reference held if the caller provides
 * a non-NULL retifp.  The caller is responsible for checking if the
 * returned ifp is valid and release its reference at all times.
 *
 * clone - meaningful only for bsdi and freebsd
 */
static int
selectroute(struct sockaddr_in6 *srcsock, struct sockaddr_in6 *dstsock,
    struct ip6_pktopts *opts, struct ip6_moptions *mopts,
    struct in6_ifaddr **retsrcia, struct route_in6 *ro,
    struct ifnet **retifp, struct rtentry **retrt, int clone,
    int norouteok, struct ip6_out_args *ip6oa)
{
	int error = 0;
	struct ifnet *__single ifp = NULL, *__single ifp0 = NULL;
	struct route_in6 *__single route = NULL;
	struct sockaddr_in6 *__single sin6_next;
	struct in6_pktinfo *__single pi = NULL;
	struct in6_addr *__single dst = &dstsock->sin6_addr;
	struct ifaddr *__single ifa = NULL;
	char s_src[MAX_IPv6_STR_LEN], s_dst[MAX_IPv6_STR_LEN];
	boolean_t select_srcif, proxied_ifa = FALSE, local_dst = FALSE;
	unsigned int ifscope = ((ip6oa != NULL) ?
	    ip6oa->ip6oa_boundif : IFSCOPE_NONE);
	boolean_t is_direct = FALSE;

	if (retifp != NULL) {
		*retifp = NULL;
	}

	if (retrt != NULL) {
		*retrt = NULL;
	}

	if (ip6_select_srcif_debug) {
		struct in6_addr src;
		src = (srcsock != NULL) ? srcsock->sin6_addr : in6addr_any;
		(void) inet_ntop(AF_INET6, &src, s_src, sizeof(s_src));
		(void) inet_ntop(AF_INET6, dst, s_dst, sizeof(s_dst));
	}

	/*
	 * If the destination address is UNSPECIFIED addr, bail out.
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(dst)) {
		error = EHOSTUNREACH;
		goto done;
	}

	/*
	 * Perform source interface selection if Scoped Routing
	 * is enabled and a source address that isn't unspecified.
	 */
	select_srcif = (srcsock != NULL &&
	    !IN6_IS_ADDR_UNSPECIFIED(&srcsock->sin6_addr));

	/*
	 * For scoped routing, if interface scope is 0 or src/dst addr is linklocal
	 * or dst addr is multicast, source interface selection should be performed even
	 * if the destination is directly reachable.
	 */
	if (ifscope != IFSCOPE_NONE &&
	    !(srcsock != NULL && IN6_IS_ADDR_LINKLOCAL(&srcsock->sin6_addr)) &&
	    !IN6_IS_ADDR_MULTICAST(dst) && !IN6_IS_ADDR_LINKLOCAL(dst)) {
		struct rtentry *__single temp_rt = NULL;

		lck_mtx_lock(rnh_lock);
		temp_rt = rt_lookup(TRUE, SA(dstsock),
		    NULL, rt_tables[AF_INET6], ifscope);
		lck_mtx_unlock(rnh_lock);

		/*
		 * If the destination is directly reachable, relax
		 * the behavior around select_srcif, i.e. don't force
		 * the packet to go out from the interface that is hosting
		 * the source address.
		 * It happens when we share v6 with NAT66 and want
		 * the external interface's v6 address to be reachable
		 * to the clients we are sharing v6 connectivity with
		 * using NAT.
		 */
		if (temp_rt != NULL) {
			if ((temp_rt->rt_flags & RTF_GATEWAY) == 0) {
				select_srcif = FALSE;
				is_direct = TRUE;
			}
			rtfree(temp_rt);
		}
	}

	if (ip6_select_srcif_debug) {
		os_log(OS_LOG_DEFAULT, "%s src %s dst %s ifscope %d "
		    "is_direct %d select_srcif %d",
		    __func__, s_src, s_dst, ifscope, is_direct, select_srcif);
	}

	/* If the caller specified the outgoing interface explicitly, use it */
	if (opts != NULL && (pi = opts->ip6po_pktinfo) != NULL &&
	    pi->ipi6_ifindex != 0) {
		/*
		 * If IPV6_PKTINFO takes precedence over IPV6_BOUND_IF.
		 */
		ifscope = pi->ipi6_ifindex;
		ifnet_head_lock_shared();
		/* ifp may be NULL if detached or out of range */
		ifp = ifp0 =
		    ((ifscope <= if_index) ? ifindex2ifnet[ifscope] : NULL);
		ifnet_head_done();
		if (norouteok || retrt == NULL || IN6_IS_ADDR_MC_LINKLOCAL(dst)) {
			/*
			 * We do not have to check or get the route for
			 * multicast.  If the caller didn't ask/care for
			 * the route and we have no interface to use,
			 * it's an error.
			 */
			if (ifp == NULL) {
				error = EHOSTUNREACH;
			}
			goto done;
		} else {
			goto getsrcif;
		}
	}

	/*
	 * If the destination address is a multicast address and the outgoing
	 * interface for the address is specified by the caller, use it.
	 */
	if (IN6_IS_ADDR_MULTICAST(dst) && mopts != NULL) {
		IM6O_LOCK(mopts);
		ifp = ifp0 = mopts->im6o_multicast_ifp;
		if (ifp != NULL && IN6_IS_ADDR_MC_LINKLOCAL(dst)) {
			IM6O_UNLOCK(mopts);
			goto done; /* we don't need a route for link-local multicast */
		}
		IM6O_UNLOCK(mopts);
	}

getsrcif:
	/*
	 * If the outgoing interface was not set via IPV6_BOUND_IF or
	 * IPV6_PKTINFO, use the scope ID in the destination address.
	 */
	if (ifscope == IFSCOPE_NONE) {
		ifscope = dstsock->sin6_scope_id;
	}

	/*
	 * Perform source interface selection; the source IPv6 address
	 * must belong to one of the addresses of the interface used
	 * by the route.  For performance reasons, do this only if
	 * there is no route, or if the routing table has changed,
	 * or if we haven't done source interface selection on this
	 * route (for this PCB instance) before.
	 */
	if (!select_srcif) {
		goto getroute;
	} else if (!ROUTE_UNUSABLE(ro) && ro->ro_srcia != NULL &&
	    (ro->ro_flags & ROF_SRCIF_SELECTED)) {
		if (ro->ro_rt->rt_ifp->if_flags & IFF_LOOPBACK) {
			local_dst = TRUE;
		}
		ifa = ro->ro_srcia;
		ifa_addref(ifa);        /* for caller */
		goto getroute;
	}

	/*
	 * Given the source IPv6 address, find a suitable source interface
	 * to use for transmission; if a scope ID has been specified,
	 * optimize the search by looking at the addresses only for that
	 * interface.  This is still suboptimal, however, as we need to
	 * traverse the per-interface list.
	 */
	if (ifscope != IFSCOPE_NONE || (ro != NULL && ro->ro_rt != NULL)) {
		unsigned int scope = ifscope;
		struct ifnet *__single rt_ifp;

		rt_ifp = (ro->ro_rt != NULL) ? ro->ro_rt->rt_ifp : NULL;

		/*
		 * If no scope is specified and the route is stale (pointing
		 * to a defunct interface) use the current primary interface;
		 * this happens when switching between interfaces configured
		 * with the same IPv6 address.  Otherwise pick up the scope
		 * information from the route; the ULP may have looked up a
		 * correct route and we just need to verify it here and mark
		 * it with the ROF_SRCIF_SELECTED flag below.
		 */
		if (scope == IFSCOPE_NONE) {
			scope = rt_ifp->if_index;
			if (scope != get_primary_ifscope(AF_INET6) &&
			    ROUTE_UNUSABLE(ro)) {
				scope = get_primary_ifscope(AF_INET6);
			}
		}

		ifa = (struct ifaddr *)
		    ifa_foraddr6_scoped(&srcsock->sin6_addr, scope);

		/*
		 * If we are forwarding and proxying prefix(es), see if the
		 * source address is one of ours and is a proxied address;
		 * if so, use it.
		 */
		if (ifa == NULL && ip6_forwarding && nd6_prproxy) {
			ifa = (struct ifaddr *)
			    ifa_foraddr6(&srcsock->sin6_addr);
			if (ifa != NULL && !(proxied_ifa =
			    nd6_prproxy_ifaddr(ifatoia6(ifa)))) {
				ifa_remref(ifa);
				ifa = NULL;
			}
		}

		if (ip6_select_srcif_debug && ifa != NULL) {
			if (ro->ro_rt != NULL) {
				os_log(OS_LOG_DEFAULT, "%s %s->%s ifscope %d->%d ifa_if %s "
				    "ro_if %s",
				    __func__,
				    s_src, s_dst, ifscope,
				    scope, if_name(ifa->ifa_ifp),
				    if_name(rt_ifp));
			} else {
				os_log(OS_LOG_DEFAULT, "%s %s->%s ifscope %d->%d ifa_if %s",
				    __func__,
				    s_src, s_dst, ifscope, scope,
				    if_name(ifa->ifa_ifp));
			}
		}
	}

	/*
	 * Slow path; search for an interface having the corresponding source
	 * IPv6 address if the scope was not specified by the caller, and:
	 *
	 *   1) There currently isn't any route, or,
	 *   2) The interface used by the route does not own that source
	 *	IPv6 address; in this case, the route will get blown away
	 *	and we'll do a more specific scoped search using the newly
	 *	found interface.
	 */
	if (ifa == NULL && ifscope == IFSCOPE_NONE) {
		struct ifaddr *__single ifadst;

		/* Check if the destination address is one of ours */
		ifadst = (struct ifaddr *)ifa_foraddr6(&dstsock->sin6_addr);
		if (ifadst != NULL) {
			local_dst = TRUE;
			ifa_remref(ifadst);
		}

		ifa = (struct ifaddr *)ifa_foraddr6(&srcsock->sin6_addr);

		if (ip6_select_srcif_debug && ifa != NULL) {
			os_log(OS_LOG_DEFAULT, "%s %s->%s ifscope %d ifa_if %s",
			    __func__,
			    s_src, s_dst, ifscope, if_name(ifa->ifa_ifp));
		} else if (ip6_select_srcif_debug) {
			os_log(OS_LOG_DEFAULT, "%s %s->%s ifscope %d ifa_if NULL",
			    __func__,
			    s_src, s_dst, ifscope);
		}
	}

getroute:
	if (ifa != NULL && !proxied_ifa && !local_dst) {
		ifscope = ifa->ifa_ifp->if_index;
	}

	/*
	 * If the next hop address for the packet is specified by the caller,
	 * use it as the gateway.
	 */
	if (opts != NULL && opts->ip6po_nexthop != NULL) {
		struct route_in6 *__single ron;

		sin6_next = satosin6(opts->ip6po_nexthop);

		/* at this moment, we only support AF_INET6 next hops */
		if (sin6_next->sin6_family != AF_INET6) {
			error = EAFNOSUPPORT; /* or should we proceed? */
			goto done;
		}

		/*
		 * If the next hop is an IPv6 address, then the node identified
		 * by that address must be a neighbor of the sending host.
		 */
		ron = &opts->ip6po_nextroute;
		if (ron->ro_rt != NULL) {
			RT_LOCK(ron->ro_rt);
		}
		if (ROUTE_UNUSABLE(ron) || (ron->ro_rt != NULL &&
		    (!(ron->ro_rt->rt_flags & RTF_LLINFO) ||
		    (select_srcif && (ifa == NULL ||
		    (ifa->ifa_ifp != ron->ro_rt->rt_ifp && !proxied_ifa))))) ||
		    !in6_are_addr_equal_scoped(&satosin6(&ron->ro_dst)->sin6_addr,
		    &sin6_next->sin6_addr, ron->ro_rt->rt_ifp->if_index, sin6_next->sin6_scope_id)) {
			if (ron->ro_rt != NULL) {
				RT_UNLOCK(ron->ro_rt);
			}

			ROUTE_RELEASE(ron);
			*satosin6(&ron->ro_dst) = *sin6_next;
		}
		if (ron->ro_rt == NULL) {
			rtalloc_scoped((struct route *)ron, ifscope);
			if (ron->ro_rt != NULL) {
				RT_LOCK(ron->ro_rt);
			}
			if (ROUTE_UNUSABLE(ron) ||
			    !(ron->ro_rt->rt_flags & RTF_LLINFO) ||
			    !in6_are_addr_equal_scoped(&satosin6(rt_key(ron->ro_rt))->
			    sin6_addr, &sin6_next->sin6_addr, ron->ro_rt->rt_ifp->if_index, sin6_next->sin6_scope_id)) {
				if (ron->ro_rt != NULL) {
					RT_UNLOCK(ron->ro_rt);
				}

				ROUTE_RELEASE(ron);
				error = EHOSTUNREACH;
				goto done;
			}
		}
		route = ron;
		ifp = ifp0 = ron->ro_rt->rt_ifp;

		/*
		 * When cloning is required, try to allocate a route to the
		 * destination so that the caller can store path MTU
		 * information.
		 */
		if (!clone) {
			if (select_srcif) {
				/* Keep the route locked */
				goto validateroute;
			}
			RT_UNLOCK(ron->ro_rt);
			goto done;
		}
		RT_UNLOCK(ron->ro_rt);
	}

	/*
	 * Use a cached route if it exists and is valid, else try to allocate
	 * a new one.  Note that we should check the address family of the
	 * cached destination, in case of sharing the cache with IPv4.
	 */
	if (ro == NULL) {
		goto done;
	}
	if (ro->ro_rt != NULL) {
		RT_LOCK_SPIN(ro->ro_rt);
	}
	if (ROUTE_UNUSABLE(ro) || (ro->ro_rt != NULL &&
	    (satosin6(&ro->ro_dst)->sin6_family != AF_INET6 ||
	    !in6_are_addr_equal_scoped(&satosin6(&ro->ro_dst)->sin6_addr, dst, ro->ro_rt->rt_ifp->if_index, dstsock->sin6_scope_id) ||
	    (select_srcif && (ifa == NULL ||
	    (ifa->ifa_ifp != ro->ro_rt->rt_ifp && !proxied_ifa)))))) {
		if (ro->ro_rt != NULL) {
			RT_UNLOCK(ro->ro_rt);
		}

		ROUTE_RELEASE(ro);
	}
	if (ro->ro_rt == NULL) {
		struct sockaddr_in6 *__single sa6;

		/* No route yet, so try to acquire one */
		SOCKADDR_ZERO(&ro->ro_dst, sizeof(struct sockaddr_in6));
		sa6 = SIN6(&ro->ro_dst);
		sa6->sin6_family = AF_INET6;
		sa6->sin6_len = sizeof(struct sockaddr_in6);
		sa6->sin6_addr = *dst;
		if (IN6_IS_ADDR_MC_LINKLOCAL(dst)) {
			ro->ro_rt = rtalloc1_scoped(
				SA(&((struct route *)ro)->ro_dst), 0, 0, ifscope);
		} else {
			rtalloc_scoped((struct route *)ro, ifscope);
		}
		if (ro->ro_rt != NULL) {
			RT_LOCK_SPIN(ro->ro_rt);
		}
	}

	/*
	 * Do not care about the result if we have the nexthop
	 * explicitly specified (in case we're asked to clone.)
	 */
	if (opts != NULL && opts->ip6po_nexthop != NULL) {
		if (ro->ro_rt != NULL) {
			RT_UNLOCK(ro->ro_rt);
		}
		goto done;
	}

	if (ro->ro_rt != NULL) {
		RT_LOCK_ASSERT_HELD(ro->ro_rt);
		ifp = ifp0 = ro->ro_rt->rt_ifp;
	} else {
		error = EHOSTUNREACH;
	}
	route = ro;

validateroute:
	if (select_srcif) {
		boolean_t has_route = (route != NULL && route->ro_rt != NULL);
		boolean_t srcif_selected = FALSE;

		if (has_route) {
			RT_LOCK_ASSERT_HELD(route->ro_rt);
		}
		/*
		 * If there is a non-loopback route with the wrong interface,
		 * or if there is no interface configured with such an address,
		 * blow it away.  Except for local/loopback, we look for one
		 * with a matching interface scope/index.
		 */
		if (has_route && (ifa == NULL ||
		    (ifa->ifa_ifp != ifp && ifp != lo_ifp) ||
		    !(route->ro_rt->rt_flags & RTF_UP))) {
			/*
			 * If the destination address belongs to a proxied
			 * prefix, relax the requirement and allow the packet
			 * to come out of the proxy interface with the source
			 * address of the real interface.
			 */
			if (ifa != NULL && proxied_ifa &&
			    (route->ro_rt->rt_flags & (RTF_UP | RTF_PROXY)) ==
			    (RTF_UP | RTF_PROXY)) {
				srcif_selected = TRUE;
			} else {
				if (ip6_select_srcif_debug) {
					if (ifa != NULL) {
						os_log(OS_LOG_DEFAULT,
						    "%s->%s ifscope %d "
						    "ro_if %s != ifa_if %s "
						    "(cached route cleared)",
						    s_src, s_dst,
						    ifscope, if_name(ifp),
						    if_name(ifa->ifa_ifp));
					} else {
						os_log(OS_LOG_DEFAULT,
						    "%s->%s ifscope %d "
						    "ro_if %s (no ifa_if "
						    "found)", s_src, s_dst,
						    ifscope, if_name(ifp));
					}
				}
				RT_UNLOCK(route->ro_rt);
				ROUTE_RELEASE(route);
				error = EHOSTUNREACH;
				/* Undo the settings done above */
				route = NULL;
				ifp = NULL;     /* ditch ifp; keep ifp0 */
				has_route = FALSE;
			}
		} else if (has_route) {
			srcif_selected = TRUE;
		}

		if (srcif_selected) {
			VERIFY(has_route);
			if (ifa != route->ro_srcia ||
			    !(route->ro_flags & ROF_SRCIF_SELECTED)) {
				RT_CONVERT_LOCK(route->ro_rt);
				if (ifa != NULL) {
					ifa_addref(ifa); /* for route_in6 */
				}
				if (route->ro_srcia != NULL) {
					ifa_remref(route->ro_srcia);
				}
				route->ro_srcia = ifa;
				route->ro_flags |= ROF_SRCIF_SELECTED;
				RT_GENID_SYNC(route->ro_rt);
			}
			RT_UNLOCK(route->ro_rt);
		}
	} else {
		if (ro->ro_rt != NULL) {
			RT_UNLOCK(ro->ro_rt);
		}
		if (ifp != NULL && opts != NULL &&
		    opts->ip6po_pktinfo != NULL &&
		    opts->ip6po_pktinfo->ipi6_ifindex != 0) {
			/*
			 * Check if the outgoing interface conflicts with the
			 * interface specified by ipi6_ifindex (if specified).
			 * Note that loopback interface is always okay.
			 * (this may happen when we are sending a packet to
			 * one of our own addresses.)
			 */
			if (!(ifp->if_flags & IFF_LOOPBACK) && ifp->if_index !=
			    opts->ip6po_pktinfo->ipi6_ifindex) {
				error = EHOSTUNREACH;
				goto done;
			}
		}
	}

done:
	/*
	 * Check for interface restrictions.
	 */
#define CHECK_RESTRICTIONS(_ip6oa, _ifp)                        \
	((((_ip6oa)->ip6oa_flags & IP6OAF_NO_CELLULAR) &&       \
	    IFNET_IS_CELLULAR(_ifp)) ||                         \
	(((_ip6oa)->ip6oa_flags & IP6OAF_NO_EXPENSIVE) &&       \
	    IFNET_IS_EXPENSIVE(_ifp)) ||                        \
	(((_ip6oa)->ip6oa_flags & IP6OAF_NO_CONSTRAINED) &&     \
	    IFNET_IS_CONSTRAINED(_ifp)) ||                      \
	(!((_ip6oa)->ip6oa_flags & IP6OAF_INTCOPROC_ALLOWED) && \
	    IFNET_IS_INTCOPROC(_ifp)) ||                        \
	(!((_ip6oa)->ip6oa_flags & IP6OAF_AWDL_UNRESTRICTED) && \
	    IFNET_IS_AWDL_RESTRICTED(_ifp)) &&                  \
	(!((_ip6oa)->ip6oa_flags & IP6OAF_MANAGEMENT_ALLOWED) && \
	    IFNET_IS_MANAGEMENT(_ifp)))

	if (error == 0 && ip6oa != NULL &&
	    ((ifp && CHECK_RESTRICTIONS(ip6oa, ifp)) ||
	    (route && route->ro_rt &&
	    CHECK_RESTRICTIONS(ip6oa, route->ro_rt->rt_ifp)))) {
		if (route != NULL && route->ro_rt != NULL) {
			ROUTE_RELEASE(route);
			route = NULL;
		}
		ifp = NULL;     /* ditch ifp; keep ifp0 */
		error = EHOSTUNREACH;
		ip6oa->ip6oa_flags |= IP6OAF_R_IFDENIED;
	}
#undef CHECK_RESTRICTIONS

	/*
	 * If the interface is disabled for IPv6, then ENETDOWN error.
	 */
	if (error == 0 &&
	    ifp != NULL && (ifp->if_eflags & IFEF_IPV6_DISABLED)) {
		error = ENETDOWN;
	}

	if (ifp == NULL && (route == NULL || route->ro_rt == NULL)) {
		/*
		 * This can happen if the caller did not pass a cached route
		 * nor any other hints.  We treat this case an error.
		 */
		error = EHOSTUNREACH;
	}
	if (error == EHOSTUNREACH || error == ENETDOWN) {
		ip6stat.ip6s_noroute++;
	}

	/*
	 * We'll return ifp regardless of error, so pick it up from ifp0
	 * in case it was nullified above.  Caller is responsible for
	 * releasing the ifp if it is non-NULL.
	 */
	ifp = ifp0;
	if (retifp != NULL) {
		if (ifp != NULL) {
			ifnet_reference(ifp);   /* for caller */
		}
		*retifp = ifp;
	}

	if (retsrcia != NULL) {
		if (ifa != NULL) {
			ifa_addref(ifa);        /* for caller */
		}
		*retsrcia = ifatoia6(ifa);
	}

	if (error == 0) {
		if (retrt != NULL && route != NULL) {
			*retrt = route->ro_rt;  /* ro_rt may be NULL */
		}
	}
	if (ip6_select_srcif_debug) {
		os_log(OS_LOG_DEFAULT,
		    "%s %s->%s ifscope %d ifa_if %s ro_if %s (error=%d)",
		    __func__,
		    s_src, s_dst, ifscope,
		    (ifa != NULL) ? if_name(ifa->ifa_ifp) : "NONE",
		    (ifp != NULL) ? if_name(ifp) : "NONE", error);
	}

	if (ifa != NULL) {
		ifa_remref(ifa);
	}

	return error;
}

/*
 * Regardless of error, it will return an ifp with a reference held if the
 * caller provides a non-NULL retifp.  The caller is responsible for checking
 * if the returned ifp is valid and release its reference at all times.
 */
int
in6_selectif(struct sockaddr_in6 *dstsock, struct ip6_pktopts *opts,
    struct ip6_moptions *mopts, struct route_in6 *ro,
    struct ip6_out_args *ip6oa, struct ifnet **retifp)
{
	int err = 0;
	struct route_in6 sro;
	struct rtentry *__single rt = NULL;

	if (ro == NULL) {
		bzero(&sro, sizeof(sro));
		ro = &sro;
	}

	if ((err = selectroute(NULL, dstsock, opts, mopts, NULL, ro, retifp,
	    &rt, 0, 1, ip6oa)) != 0) {
		goto done;
	}

	/*
	 * do not use a rejected or black hole route.
	 * XXX: this check should be done in the L2 output routine.
	 * However, if we skipped this check here, we'd see the following
	 * scenario:
	 * - install a rejected route for a scoped address prefix
	 *   (like fe80::/10)
	 * - send a packet to a destination that matches the scoped prefix,
	 *   with ambiguity about the scope zone.
	 * - pick the outgoing interface from the route, and disambiguate the
	 *   scope zone with the interface.
	 * - ip6_output() would try to get another route with the "new"
	 *   destination, which may be valid.
	 * - we'd see no error on output.
	 * Although this may not be very harmful, it should still be confusing.
	 * We thus reject the case here.
	 */
	if (rt && (rt->rt_flags & (RTF_REJECT | RTF_BLACKHOLE))) {
		err = ((rt->rt_flags & RTF_HOST) ? EHOSTUNREACH : ENETUNREACH);
		goto done;
	}

	/*
	 * Adjust the "outgoing" interface.  If we're going to loop the packet
	 * back to ourselves, the ifp would be the loopback interface.
	 * However, we'd rather know the interface associated to the
	 * destination address (which should probably be one of our own
	 * addresses.)
	 */
	if (rt != NULL && rt->rt_ifa != NULL && rt->rt_ifa->ifa_ifp != NULL &&
	    retifp != NULL) {
		ifnet_reference(rt->rt_ifa->ifa_ifp);
		if (*retifp != NULL) {
			ifnet_release(*retifp);
		}
		*retifp = rt->rt_ifa->ifa_ifp;
	}

done:
	if (ro == &sro) {
		VERIFY(rt == NULL || rt == ro->ro_rt);
		ROUTE_RELEASE(ro);
	}

	/*
	 * retifp might point to a valid ifp with a reference held;
	 * caller is responsible for releasing it if non-NULL.
	 */
	return err;
}

/*
 * Regardless of error, it will return an ifp with a reference held if the
 * caller provides a non-NULL retifp.  The caller is responsible for checking
 * if the returned ifp is valid and release its reference at all times.
 *
 * clone - meaningful only for bsdi and freebsd
 */
int
in6_selectroute(struct sockaddr_in6 *srcsock, struct sockaddr_in6 *dstsock,
    struct ip6_pktopts *opts, struct ip6_moptions *mopts,
    struct in6_ifaddr **retsrcia, struct route_in6 *ro, struct ifnet **retifp,
    struct rtentry **retrt, int clone, struct ip6_out_args *ip6oa)
{
	return selectroute(srcsock, dstsock, opts, mopts, retsrcia, ro, retifp,
	           retrt, clone, 0, ip6oa);
}

/*
 * Default hop limit selection. The precedence is as follows:
 * 1. Hoplimit value specified via socket option.
 * 2. (If the outgoing interface is detected) the current
 *     hop limit of the interface specified by router advertisement.
 * 3. The system default hoplimit.
 */
uint8_t
in6_selecthlim(struct in6pcb *in6p, struct ifnet *ifp)
{
	if (in6p && in6p->in6p_hops >= 0) {
		return (uint8_t)in6p->in6p_hops;
	} else if (NULL != ifp) {
		uint8_t chlim;
		struct nd_ifinfo *__single ndi = ND_IFINFO(ifp);
		if (ndi && ndi->initialized) {
			/* access chlim without lock, for performance */
			chlim = ndi->chlim;
		} else {
			chlim = (uint8_t)ip6_defhlim;
		}
		return chlim;
	}

	return (uint8_t)ip6_defhlim;
}

/*
 * XXX: this is borrowed from in6_pcbbind(). If possible, we should
 * share this function by all *bsd*...
 */
int
in6_pcbsetport(struct in6_addr *laddr, struct sockaddr *remote, struct inpcb *inp, struct proc *p,
    int locked)
{
	struct socket *__single so = inp->inp_socket;
	uint16_t lport = 0, first, last, rand_port;
	uint16_t *__single lastport;
	int count, error = 0, wild = 0;
	boolean_t counting_down;
	bool found, randomport;
	struct inpcbinfo *__single pcbinfo = inp->inp_pcbinfo;
	kauth_cred_t __single cred;
#if SKYWALK
	bool laddr_unspecified = IN6_IS_ADDR_UNSPECIFIED(laddr);
#else
#pragma unused(laddr)
#endif
	if (!locked) { /* Make sure we don't run into a deadlock: 4052373 */
		if (!lck_rw_try_lock_exclusive(&pcbinfo->ipi_lock)) {
			socket_unlock(inp->inp_socket, 0);
			lck_rw_lock_exclusive(&pcbinfo->ipi_lock);
			socket_lock(inp->inp_socket, 0);
		}

		/*
		 * Check if a local port was assigned to the inp while
		 * this thread was waiting for the pcbinfo lock
		 */
		if (inp->inp_lport != 0) {
			VERIFY(inp->inp_flags2 & INP2_INHASHLIST);
			lck_rw_done(&pcbinfo->ipi_lock);

			/*
			 * It is not an error if another thread allocated
			 * a port
			 */
			return 0;
		}
	}

	/* XXX: this is redundant when called from in6_pcbbind */
	if ((so->so_options & (SO_REUSEADDR | SO_REUSEPORT)) == 0) {
		wild = INPLOOKUP_WILDCARD;
	}

	randomport = (so->so_flags & SOF_BINDRANDOMPORT) > 0 ||
	    (so->so_type == SOCK_STREAM ? tcp_use_randomport :
	    udp_use_randomport) > 0;

	if (inp->inp_flags & INP_HIGHPORT) {
		first = (uint16_t)ipport_hifirstauto;     /* sysctl */
		last  = (uint16_t)ipport_hilastauto;
		lastport = &pcbinfo->ipi_lasthi;
	} else if (inp->inp_flags & INP_LOWPORT) {
		cred = kauth_cred_proc_ref(p);
		error = priv_check_cred(cred, PRIV_NETINET_RESERVEDPORT, 0);
		kauth_cred_unref(&cred);
		if (error != 0) {
			if (!locked) {
				lck_rw_done(&pcbinfo->ipi_lock);
			}
			return error;
		}
		first = (uint16_t)ipport_lowfirstauto;    /* 1023 */
		last  = (uint16_t)ipport_lowlastauto;     /* 600 */
		lastport = &pcbinfo->ipi_lastlow;
	} else {
		first = (uint16_t)ipport_firstauto;       /* sysctl */
		last  = (uint16_t)ipport_lastauto;
		lastport = &pcbinfo->ipi_lastport;
	}

	if (first == last) {
		randomport = false;
	}
	/*
	 * Simple check to ensure all ports are not used up causing
	 * a deadlock here.
	 */
	found = false;
	if (first > last) {
		/* counting down */
		if (randomport) {
			read_frandom(&rand_port, sizeof(rand_port));
			*lastport = first - (rand_port % (first - last));
		}
		count = first - last;
		counting_down = TRUE;
	} else {
		/* counting up */
		if (randomport) {
			read_frandom(&rand_port, sizeof(rand_port));
			*lastport = first + (rand_port % (first - last));
		}
		count = last - first;
		counting_down = FALSE;
	}
	do {
		if (count-- < 0) {      /* completely used? */
			/*
			 * Undo any address bind that may have
			 * occurred above.
			 */
			inp->in6p_laddr = in6addr_any;
			inp->in6p_last_outifp = NULL;
			inp->inp_lifscope = IFSCOPE_NONE;
#if SKYWALK
			if (NETNS_TOKEN_VALID(&inp->inp_netns_token)) {
				netns_set_ifnet(&inp->inp_netns_token,
				    NULL);
			}
#endif /* SKYWALK */
			if (!locked) {
				lck_rw_done(&pcbinfo->ipi_lock);
			}
			return EAGAIN;
		}
		if (counting_down) {
			--*lastport;
			if (*lastport > first || *lastport < last) {
				*lastport = first;
			}
		} else {
			++*lastport;
			if (*lastport < first || *lastport > last) {
				*lastport = first;
			}
		}
		lport = htons(*lastport);

		/*
		 * Skip if this is a restricted port as we do not want to
		 * restricted ports as ephemeral
		 */
		if (IS_RESTRICTED_IN_PORT(lport)) {
			continue;
		}

		found = (in6_pcblookup_local(pcbinfo, &inp->in6p_laddr,
		    lport, inp->inp_lifscope, wild) == NULL);
#if SKYWALK
		if (found &&
		    (SOCK_PROTO(so) == IPPROTO_TCP ||
		    SOCK_PROTO(so) == IPPROTO_UDP) &&
		    !(inp->inp_flags2 & INP2_EXTERNAL_PORT)) {
			if (laddr_unspecified &&
			    (inp->inp_flags & IN6P_IPV6_V6ONLY) == 0) {
				struct in_addr ip_zero = { .s_addr = 0 };

				netns_release(&inp->inp_wildcard_netns_token);
				if (netns_reserve_in(
					    &inp->inp_wildcard_netns_token,
					    ip_zero,
					    (uint8_t)SOCK_PROTO(so), lport,
					    NETNS_BSD, NULL) != 0) {
					/* port in use in IPv4 namespace */
					found = false;
				}
			}
			if (found &&
			    netns_reserve_in6(&inp->inp_netns_token,
			    inp->in6p_laddr, (uint8_t)SOCK_PROTO(so), lport,
			    NETNS_BSD, NULL) != 0) {
				netns_release(&inp->inp_wildcard_netns_token);
				found = false;
			}
		}
#endif /* SKYWALK */
	} while (!found);

	inp->inp_lport = lport;
	inp->inp_flags |= INP_ANONPORT;

	if (in_pcbinshash(inp, remote, 1) != 0) {
		inp->in6p_laddr = in6addr_any;
		inp->in6p_last_outifp = NULL;
		inp->inp_lifscope = IFSCOPE_NONE;
#if SKYWALK
		netns_release(&inp->inp_netns_token);
#endif /* SKYWALK */
		inp->inp_lport = 0;
		inp->inp_flags &= ~INP_ANONPORT;
		if (!locked) {
			lck_rw_done(&pcbinfo->ipi_lock);
		}
		return EAGAIN;
	}

	if (!locked) {
		lck_rw_done(&pcbinfo->ipi_lock);
	}
	return 0;
}

/*
 * The followings are implementation of the policy table using a
 * simple tail queue.
 * XXX such details should be hidden.
 * XXX implementation using binary tree should be more efficient.
 */
struct addrsel_policyent {
	TAILQ_ENTRY(addrsel_policyent) ape_entry;
	struct in6_addrpolicy ape_policy;
};

TAILQ_HEAD(addrsel_policyhead, addrsel_policyent);

struct addrsel_policyhead addrsel_policytab;

static void
init_policy_queue(void)
{
	TAILQ_INIT(&addrsel_policytab);
}

void
addrsel_policy_init(void)
{
	/*
	 * Default address selection policy based on RFC 6724.
	 */
	static const struct in6_addrpolicy defaddrsel[] = {
		/* Loopback -- prefix=::1/128, precedence=50, label=0 */
		{
			.addr = {
				.sin6_family = AF_INET6,
				.sin6_addr   = IN6ADDR_LOOPBACK_INIT,
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.addrmask = {
				.sin6_family = AF_INET6,
				.sin6_addr   = IN6MASK128,
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.preced   = 50,
			.label    = 0
		},

		/* Unspecified -- prefix=::/0, precedence=40, label=1 */
		{
			.addr = {
				.sin6_family = AF_INET6,
				.sin6_addr   = IN6ADDR_ANY_INIT,
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.addrmask = {
				.sin6_family = AF_INET6,
				.sin6_addr   = IN6MASK0,
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.preced   = 40,
			.label    = 1
		},

		/* IPv4 Mapped -- prefix=::ffff:0:0/96, precedence=35, label=4 */
		{
			.addr = {
				.sin6_family = AF_INET6,
				.sin6_addr   = IN6ADDR_V4MAPPED_INIT,
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.addrmask = {
				.sin6_family = AF_INET6,
				.sin6_addr   = IN6MASK96,
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.preced   = 35,
			.label    = 4
		},

		/* 6to4 -- prefix=2002::/16, precedence=30, label=2 */
		{
			.addr = {
				.sin6_family = AF_INET6,
				.sin6_addr   = {{{ 0x20, 0x02 }}},
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.addrmask = {
				.sin6_family = AF_INET6,
				.sin6_addr   = IN6MASK16,
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.preced   = 30,
			.label    = 2
		},

		/* Teredo -- prefix=2001::/32, precedence=5, label=5 */
		{
			.addr = {
				.sin6_family = AF_INET6,
				.sin6_addr   = {{{ 0x20, 0x01 }}},
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.addrmask = {
				.sin6_family = AF_INET6,
				.sin6_addr   = IN6MASK32,
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.preced   = 5,
			.label    = 5
		},

		/* Unique Local (ULA) -- prefix=fc00::/7, precedence=3, label=13 */
		{
			.addr = {
				.sin6_family = AF_INET6,
				.sin6_addr   = {{{ 0xfc }}},
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.addrmask = {
				.sin6_family = AF_INET6,
				.sin6_addr   = IN6MASK7,
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.preced   = 3,
			.label    = 13
		},

		/* IPv4 Compatible -- prefix=::/96, precedence=1, label=3 */
		{
			.addr = {
				.sin6_family = AF_INET6,
				.sin6_addr = IN6ADDR_ANY_INIT,
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.addrmask = {
				.sin6_family = AF_INET6,
				.sin6_addr = IN6MASK96,
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.preced   = 1,
			.label    = 3
		},

		/* Site-local (deprecated) -- prefix=fec0::/10, precedence=1, label=11 */
		{
			.addr = {
				.sin6_family = AF_INET6,
				.sin6_addr = {{{ 0xfe, 0xc0 }}},
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.addrmask = {
				.sin6_family = AF_INET6,
				.sin6_addr = IN6MASK16,
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.preced   = 1,
			.label    = 11
		},

		/* 6bone (deprecated) -- prefix=3ffe::/16, precedence=1, label=12 */
		{
			.addr = {
				.sin6_family = AF_INET6,
				.sin6_addr = {{{ 0x3f, 0xfe }}},
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.addrmask = {
				.sin6_family = AF_INET6,
				.sin6_addr = IN6MASK16,
				.sin6_len    = sizeof(struct sockaddr_in6)
			},
			.preced   = 1,
			.label    = 12
		},
	};
	int i;

	init_policy_queue();

	/* initialize the "last resort" policy */
	bzero(&defaultaddrpolicy, sizeof(defaultaddrpolicy));
	defaultaddrpolicy.label = ADDR_LABEL_NOTAPP;

	for (i = 0; i < sizeof(defaddrsel) / sizeof(defaddrsel[0]); i++) {
		add_addrsel_policyent(&defaddrsel[i]);
	}
}

struct in6_addrpolicy *
in6_addrsel_lookup_policy(struct sockaddr_in6 *key)
{
	struct in6_addrpolicy *__single match = NULL;

	match = match_addrsel_policy(key);

	if (match == NULL) {
		match = &defaultaddrpolicy;
	} else {
		match->use++;
	}

	return match;
}

static struct in6_addrpolicy *
match_addrsel_policy(struct sockaddr_in6 *key)
{
	struct addrsel_policyent *__single pent;
	struct in6_addrpolicy *__single bestpol = NULL, *__single pol;
	int matchlen, bestmatchlen = -1;
	u_char *mp, *ep, *k, *p, m;

	TAILQ_FOREACH(pent, &addrsel_policytab, ape_entry) {
		matchlen = 0;

		pol = &pent->ape_policy;
		mp = (u_char *)&pol->addrmask.sin6_addr;
		ep = mp + 16;   /* XXX: scope field? */
		k = (u_char *)&key->sin6_addr;
		p = (u_char *)&pol->addr.sin6_addr;
		for (; mp < ep && *mp; mp++, k++, p++) {
			m = *mp;
			if ((*k & m) != *p) {
				goto next; /* not match */
			}
			if (m == 0xff) { /* short cut for a typical case */
				matchlen += 8;
			} else {
				while (m >= 0x80) {
					matchlen++;
					m = (u_char)(m << 1);
				}
			}
		}

		/* matched.  check if this is better than the current best. */
		if (bestpol == NULL ||
		    matchlen > bestmatchlen) {
			bestpol = pol;
			bestmatchlen = matchlen;
		}

next:
		continue;
	}

	return bestpol;
}

static int
add_addrsel_policyent(const struct in6_addrpolicy *newpolicy)
{
	struct addrsel_policyent *__single new, *__single pol;

	new = kalloc_type(struct addrsel_policyent, Z_WAITOK | Z_ZERO);

	/* duplication check */
	TAILQ_FOREACH(pol, &addrsel_policytab, ape_entry) {
		if (IN6_ARE_ADDR_EQUAL(&newpolicy->addr.sin6_addr,
		    &pol->ape_policy.addr.sin6_addr) &&
		    IN6_ARE_ADDR_EQUAL(&newpolicy->addrmask.sin6_addr,
		    &pol->ape_policy.addrmask.sin6_addr)) {
			kfree_type(struct addrsel_policyent, new);
			return EEXIST;        /* or override it? */
		}
	}

	/* XXX: should validate entry */
	new->ape_policy = *newpolicy;

	TAILQ_INSERT_TAIL(&addrsel_policytab, new, ape_entry);

	return 0;
}

int
walk_addrsel_policy(int (*callback)(const struct in6_addrpolicy *, void *),
    void *w)
{
	struct addrsel_policyent *__single pol;
	int error = 0;

	TAILQ_FOREACH(pol, &addrsel_policytab, ape_entry) {
		if ((error = (*callback)(&pol->ape_policy, w)) != 0) {
			return error;
		}
	}
	return error;
}
/*
 * Subroutines to manage the address selection policy table via sysctl.
 */
struct walkarg {
	struct sysctl_req *w_req;
};


static int
dump_addrsel_policyent(const struct in6_addrpolicy *pol, void *arg)
{
	int error = 0;
	struct walkarg *__single w = arg;

	error = SYSCTL_OUT(w->w_req, pol, sizeof(*pol));

	return error;
}

static int
in6_src_sysctl SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	struct walkarg w;

	if (req->newptr) {
		return EPERM;
	}
	bzero(&w, sizeof(w));
	w.w_req = req;

	return walk_addrsel_policy(dump_addrsel_policyent, &w);
}


SYSCTL_NODE(_net_inet6_ip6, IPV6CTL_ADDRCTLPOLICY, addrctlpolicy,
    CTLFLAG_RD | CTLFLAG_LOCKED, in6_src_sysctl, "");
int
in6_src_ioctl(u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data)
{
	int i;
	struct in6_addrpolicy ent0;

	if (cmd != SIOCAADDRCTL_POLICY && cmd != SIOCDADDRCTL_POLICY) {
		return EOPNOTSUPP; /* check for safety */
	}
	bcopy(data, &ent0, sizeof(ent0));

	if (ent0.label == ADDR_LABEL_NOTAPP) {
		return EINVAL;
	}
	/* check if the prefix mask is consecutive. */
	if (in6_mask2len(&ent0.addrmask.sin6_addr, NULL) < 0) {
		return EINVAL;
	}
	/* clear trailing garbages (if any) of the prefix address. */
	for (i = 0; i < 4; i++) {
		ent0.addr.sin6_addr.s6_addr32[i] &=
		    ent0.addrmask.sin6_addr.s6_addr32[i];
	}
	ent0.use = 0;

	switch (cmd) {
	case SIOCAADDRCTL_POLICY:
		return ENOTSUP;
	case SIOCDADDRCTL_POLICY:
		return ENOTSUP;
	}

	return 0;             /* XXX: compromise compilers */
}

/*
 * generate kernel-internal form (scopeid embedded into s6_addr16[1]).
 * If the address scope of is link-local, embed the interface index in the
 * address.  The routine determines our precedence
 * between advanced API scope/interface specification and basic API
 * specification.
 *
 * this function should be nuked in the future, when we get rid of
 * embedded scopeid thing.
 *
 * XXX actually, it is over-specification to return ifp against sin6_scope_id.
 * there can be multiple interfaces that belong to a particular scope zone
 * (in specification, we have 1:N mapping between a scope zone and interfaces).
 * we may want to change the function to return something other than ifp.
 */
int
in6_embedscope(struct in6_addr *in6, const struct sockaddr_in6 *sin6,
    struct in6pcb *in6p, struct ifnet **ifpp, struct ip6_pktopts *opt, uint32_t *ret_ifscope)
{
	struct ifnet *__single ifp = NULL;
	u_int32_t scopeid;
	struct ip6_pktopts *__single optp = NULL;

	*in6 = sin6->sin6_addr;
	scopeid = sin6->sin6_scope_id;
	if (ifpp != NULL) {
		*ifpp = NULL;
	}

	/*
	 * don't try to read sin6->sin6_addr beyond here, since the caller may
	 * ask us to overwrite existing sockaddr_in6
	 */

#ifdef ENABLE_DEFAULT_SCOPE
	if (scopeid == 0) {
		scopeid = scope6_addr2default(in6);
	}
#endif

	if (IN6_IS_SCOPE_LINKLOCAL(in6) || IN6_IS_ADDR_MC_INTFACELOCAL(in6)) {
		struct in6_pktinfo *__single pi;
		struct ifnet *__single im6o_multicast_ifp = NULL;

		if (in6p != NULL && IN6_IS_ADDR_MULTICAST(in6) &&
		    in6p->in6p_moptions != NULL) {
			IM6O_LOCK(in6p->in6p_moptions);
			im6o_multicast_ifp =
			    in6p->in6p_moptions->im6o_multicast_ifp;
			IM6O_UNLOCK(in6p->in6p_moptions);
		}

		if (opt != NULL) {
			optp = opt;
		} else if (in6p != NULL) {
			optp = in6p->in6p_outputopts;
		}
		/*
		 * KAME assumption: link id == interface id
		 */
		if (in6p != NULL && optp != NULL &&
		    (pi = optp->ip6po_pktinfo) != NULL &&
		    pi->ipi6_ifindex != 0) {
			/* ifp is needed here if only we're returning it */
			if (ifpp != NULL) {
				ifnet_head_lock_shared();
				ifp = ifindex2ifnet[pi->ipi6_ifindex];
				ifnet_head_done();
			}

			if (in6_embedded_scope) {
				in6->s6_addr16[1] = htons((uint16_t)pi->ipi6_ifindex);
			}
			if (ret_ifscope != NULL) {
				*ret_ifscope = pi->ipi6_ifindex;
			}
		} else if (in6p != NULL && IN6_IS_ADDR_MULTICAST(in6) &&
		    in6p->in6p_moptions != NULL && im6o_multicast_ifp != NULL) {
			ifp = im6o_multicast_ifp;
			if (in6_embedded_scope) {
				in6->s6_addr16[1] = htons(ifp->if_index);
			}
			if (ret_ifscope != NULL) {
				*ret_ifscope = ifp->if_index;
			}
		} else if (scopeid != 0) {
			/*
			 * Since scopeid is unsigned, we only have to check it
			 * against if_index (ifnet_head_lock not needed since
			 * if_index is an ever-increasing integer.)
			 */
			if (!IF_INDEX_IN_RANGE(scopeid)) {
				return ENXIO;  /* XXX EINVAL? */
			}
			/* ifp is needed here only if we're returning it */
			if (ifpp != NULL) {
				ifnet_head_lock_shared();
				ifp = ifindex2ifnet[scopeid];
				ifnet_head_done();
			}
			if (in6_embedded_scope) {
				/* XXX assignment to 16bit from 32bit variable */
				in6->s6_addr16[1] = htons(scopeid & 0xffff);
			}
			if (ret_ifscope != NULL) {
				*ret_ifscope = scopeid;
			}
		}

		if (ifpp != NULL) {
			if (ifp != NULL) {
				ifnet_reference(ifp);   /* for caller */
			}
			*ifpp = ifp;
		}
	}

	return 0;
}

/*
 * generate standard sockaddr_in6 from embedded form.
 * touches sin6_addr and sin6_scope_id only.
 *
 * this function should be nuked in the future, when we get rid of
 * embedded scopeid thing.
 */
int
in6_recoverscope(
	struct sockaddr_in6 *sin6,
	const struct in6_addr *in6,
	struct ifnet *ifp)
{
	u_int32_t scopeid;

	sin6->sin6_addr = *in6;

	if (!in6_embedded_scope) {
		if (ifp != NULL && IN6_IS_SCOPE_EMBED(in6)) {
			sin6->sin6_scope_id = ifp->if_index;
		}
		return 0;
	}
	/*
	 * don't try to read *in6 beyond here, since the caller may
	 * ask us to overwrite existing sockaddr_in6
	 */

	sin6->sin6_scope_id = 0;
	if (IN6_IS_SCOPE_LINKLOCAL(in6) || IN6_IS_ADDR_MC_INTFACELOCAL(in6)) {
		/*
		 * KAME assumption: link id == interface id
		 */
		scopeid = ntohs(sin6->sin6_addr.s6_addr16[1]);
		if (scopeid) {
			/*
			 * sanity check
			 *
			 * Since scopeid is unsigned, we only have to check it
			 * against if_index
			 */
			if (!IF_INDEX_IN_RANGE(scopeid)) {
				return ENXIO;
			}
			if (ifp && ifp->if_index != scopeid) {
				return ENXIO;
			}
			sin6->sin6_addr.s6_addr16[1] = 0;
			sin6->sin6_scope_id = scopeid;
		}
	}

	return 0;
}
