/*
 * Copyright (c) 1998-2023 Apple Inc. All rights reserved.
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
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)uipc_domain.c	8.3 (Berkeley) 2/14/95
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mcache.h>
#include <sys/mbuf.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc_internal.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/queue.h>

#include <net/dlil.h>
#include <net/nwk_wq.h>
#include <net/sockaddr_utils.h>

#include <mach/boolean.h>
#include <pexpert/pexpert.h>

#include <net/sockaddr_utils.h>

#if __has_ptrcheck
#include <machine/trap.h> /* Needed by bound-checks-soft when enabled. */
#endif /* __has_ptrcheck */

/* Eventhandler context for protocol events */
struct eventhandler_lists_ctxt protoctl_evhdlr_ctxt;

static void pr_init_old(struct protosw *, struct domain *);
static void init_proto(struct protosw *, struct domain *);
static void attach_proto(struct protosw *, struct domain *);
static void detach_proto(struct protosw *, struct domain *);
static void dom_init_old(struct domain *);
static void init_domain(struct domain *);
static void attach_domain(struct domain *);
static void detach_domain(struct domain *);
static struct protosw *pffindprotonotype_locked(int, int, int);
static struct domain *pffinddomain_locked(int);

static boolean_t domain_timeout_run;    /* domain timer is scheduled to run */
static boolean_t domain_draining;
static void domain_sched_timeout(void);
static void domain_timeout(void *);

static LCK_GRP_DECLARE(domain_proto_mtx_grp, "domain");
static LCK_ATTR_DECLARE(domain_proto_mtx_attr, 0, 0);
static LCK_MTX_DECLARE_ATTR(domain_proto_mtx,
    &domain_proto_mtx_grp, &domain_proto_mtx_attr);
static LCK_MTX_DECLARE_ATTR(domain_timeout_mtx,
    &domain_proto_mtx_grp, &domain_proto_mtx_attr);

uint64_t _net_uptime;
uint64_t _net_uptime_ms;
uint64_t _net_uptime_us;

#if (DEVELOPMENT || DEBUG)

SYSCTL_DECL(_kern_ipc);

static int sysctl_do_drain_domains SYSCTL_HANDLER_ARGS;

SYSCTL_PROC(_kern_ipc, OID_AUTO, do_drain_domains,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0,
    sysctl_do_drain_domains, "I", "force manual drain domains");

#endif /* DEVELOPMENT || DEBUG */

static void
pr_init_old(struct protosw *pp, struct domain *dp)
{
#pragma unused(dp)
	VERIFY(pp->pr_flags & PR_OLD);
	VERIFY(pp->pr_old != NULL);

	if (pp->pr_old->pr_init != NULL) {
		pp->pr_old->pr_init();
	}
}

static void
init_proto(struct protosw *pp, struct domain *dp)
{
	VERIFY(pp->pr_flags & PR_ATTACHED);

	if (!(pp->pr_flags & PR_INITIALIZED)) {
		TAILQ_INIT(&pp->pr_filter_head);
		if (pp->pr_init != NULL) {
			pp->pr_init(pp, dp);
		}
		pp->pr_flags |= PR_INITIALIZED;
	}
}

static void
attach_proto(struct protosw *pp, struct domain *dp)
{
	domain_proto_mtx_lock_assert_held();
	VERIFY(!(pp->pr_flags & PR_ATTACHED));
	VERIFY(pp->pr_domain == NULL);
	VERIFY(pp->pr_protosw == NULL);

	TAILQ_INSERT_TAIL(&dp->dom_protosw, pp, pr_entry);
	pp->pr_flags |= PR_ATTACHED;
	pp->pr_domain = dp;
	pp->pr_protosw = pp;

	/* do some cleaning up on user request callbacks */
	pru_sanitize(pp->pr_usrreqs);
}

static void
detach_proto(struct protosw *pp, struct domain *dp)
{
	domain_proto_mtx_lock_assert_held();
	VERIFY(pp->pr_flags & PR_ATTACHED);
	VERIFY(pp->pr_domain == dp);
	VERIFY(pp->pr_protosw == pp);

	TAILQ_REMOVE(&dp->dom_protosw, pp, pr_entry);
	pp->pr_flags &= ~PR_ATTACHED;
	pp->pr_domain = NULL;
	pp->pr_protosw = NULL;
}

static void
dom_init_old(struct domain *dp)
{
	VERIFY(dp->dom_flags & DOM_OLD);
	VERIFY(dp->dom_old != NULL);

	if (dp->dom_old->dom_init != NULL) {
		dp->dom_old->dom_init();
	}
}

static void
init_domain(struct domain *dp)
{
	VERIFY(dp->dom_flags & DOM_ATTACHED);

	if (!(dp->dom_flags & DOM_INITIALIZED)) {
		lck_mtx_init(&dp->dom_mtx_s, &domain_proto_mtx_grp,
		    &domain_proto_mtx_attr);
		dp->dom_mtx = &dp->dom_mtx_s;
		TAILQ_INIT(&dp->dom_protosw);
		if (dp->dom_init != NULL) {
			dp->dom_init(dp);
		}
		dp->dom_flags |= DOM_INITIALIZED;
	}

	/* Recompute for new protocol */
	if (max_linkhdr < 16) {        /* XXX - Sheesh; everything's ether? */
		max_linkhdr = 16;
	}
	max_linkhdr = (int)P2ROUNDUP(max_linkhdr, sizeof(uint32_t));

	if (dp->dom_protohdrlen > max_protohdr) {
		max_protohdr = dp->dom_protohdrlen;
	}
	max_protohdr = (int)P2ROUNDUP(max_protohdr, sizeof(uint32_t));

	max_hdr = max_linkhdr + max_protohdr;
	max_datalen = MHLEN - max_hdr;
}

static void
attach_domain(struct domain *dp)
{
	domain_proto_mtx_lock_assert_held();
	VERIFY(!(dp->dom_flags & DOM_ATTACHED));

	TAILQ_INSERT_TAIL(&domains, dp, dom_entry);
	dp->dom_flags |= DOM_ATTACHED;
}

static void
detach_domain(struct domain *dp)
{
	domain_proto_mtx_lock_assert_held();
	VERIFY(dp->dom_flags & DOM_ATTACHED);

	TAILQ_REMOVE(&domains, dp, dom_entry);
	dp->dom_flags &= ~DOM_ATTACHED;

	if (dp->dom_flags & DOM_OLD) {
		struct domain_old *odp = dp->dom_old;

		VERIFY(odp != NULL);
		odp->dom_next = NULL;
		odp->dom_mtx = NULL;
	}
}

/*
 * Exported (private) routine, indirection of net_add_domain.
 */
void
net_add_domain_old(struct domain_old *odp)
{
	struct domain *dp;
	domain_guard_t guard __single;

	VERIFY(odp != NULL);

	guard = domain_guard_deploy();
	if ((dp = pffinddomain_locked(odp->dom_family)) != NULL) {
		/*
		 * There is really nothing better than to panic here,
		 * as the caller would not have been able to handle
		 * any failures otherwise.
		 */
		panic("%s: domain (%d,%s) already exists for %s", __func__,
		    dp->dom_family, dp->dom_name, odp->dom_name);
		/* NOTREACHED */
	}

	/* Make sure nothing is currently pointing to the odp. */
	TAILQ_FOREACH(dp, &domains, dom_entry) {
		if (dp->dom_old == odp) {
			panic("%s: domain %p (%d,%s) is already "
			    "associated with %p (%d,%s)\n", __func__,
			    odp, odp->dom_family, odp->dom_name, dp,
			    dp->dom_family, dp->dom_name);
			/* NOTREACHED */
		}
	}

	if (odp->dom_protosw != NULL) {
		panic("%s: domain (%d,%s) protocols need to added "
		    "via net_add_proto\n", __func__, odp->dom_family,
		    odp->dom_name);
		/* NOTREACHED */
	}

	dp = kalloc_type(struct domain, Z_WAITOK | Z_ZERO | Z_NOFAIL);

	/* Copy everything but dom_init, dom_mtx, dom_next and dom_refs */
	dp->dom_family          = odp->dom_family;
	dp->dom_flags           = (odp->dom_flags & DOMF_USERFLAGS) | DOM_OLD;
	dp->dom_name            = odp->dom_name;
	dp->dom_init            = dom_init_old;
	dp->dom_externalize     = odp->dom_externalize;
	dp->dom_dispose         = odp->dom_dispose;
	dp->dom_rtattach        = odp->dom_rtattach;
	dp->dom_rtoffset        = odp->dom_rtoffset;
	dp->dom_maxrtkey        = odp->dom_maxrtkey;
	dp->dom_protohdrlen     = odp->dom_protohdrlen;
	dp->dom_old             = odp;

	attach_domain(dp);
	init_domain(dp);

	/* Point the mutex back to the internal structure's */
	odp->dom_mtx            = dp->dom_mtx;
	domain_guard_release(guard);
}

/*
 * Exported (private) routine, indirection of net_del_domain.
 */
int
net_del_domain_old(struct domain_old *odp)
{
	struct domain *dp1 __single, *dp2 __single;
	int error = 0;
	domain_guard_t guard __single;

	VERIFY(odp != NULL);

	guard = domain_guard_deploy();
	if (odp->dom_refs != 0) {
		error = EBUSY;
		goto done;
	}

	TAILQ_FOREACH_SAFE(dp1, &domains, dom_entry, dp2) {
		if (!(dp1->dom_flags & DOM_OLD)) {
			continue;
		}
		VERIFY(dp1->dom_old != NULL);
		if (odp == dp1->dom_old) {
			break;
		}
	}
	if (dp1 != NULL) {
		struct protosw *pp1 __single, *pp2 __single;

		VERIFY(dp1->dom_flags & DOM_OLD);
		VERIFY(dp1->dom_old == odp);

		/* Remove all protocols attached to this domain */
		TAILQ_FOREACH_SAFE(pp1, &dp1->dom_protosw, pr_entry, pp2) {
			detach_proto(pp1, dp1);
			if (pp1->pr_usrreqs->pru_flags & PRUF_OLD) {
				kfree_type(struct pr_usrreqs, pp1->pr_usrreqs);
			}
			if (pp1->pr_flags & PR_OLD) {
				kfree_type(struct protosw, pp1);
			}
		}

		detach_domain(dp1);
		kfree_type(struct domain, dp1);
	} else {
		error = EPFNOSUPPORT;
	}
done:
	domain_guard_release(guard);
	return error;
}

/*
 * Internal routine, not exported.
 *
 * net_add_proto - link a protosw into a domain's protosw chain
 *
 * NOTE: Caller must have acquired domain_proto_mtx
 */
int
net_add_proto(struct protosw *pp, struct domain *dp, int doinit)
{
	struct protosw *pp1;

	/*
	 * This could be called as part of initializing the domain,
	 * and thus DOM_INITIALIZED may not be set (yet).
	 */
	domain_proto_mtx_lock_assert_held();
	VERIFY(!(pp->pr_flags & PR_ATTACHED));

	/* pr_domain is set only after the protocol is attached */
	if (pp->pr_domain != NULL) {
		panic("%s: domain (%d,%s), proto %d has non-NULL pr_domain!",
		    __func__, dp->dom_family, dp->dom_name, pp->pr_protocol);
		/* NOTREACHED */
	}

	if (pp->pr_usrreqs == NULL) {
		panic("%s: domain (%d,%s), proto %d has no usrreqs!",
		    __func__, dp->dom_family, dp->dom_name, pp->pr_protocol);
		/* NOTREACHED */
	}

	TAILQ_FOREACH(pp1, &dp->dom_protosw, pr_entry) {
		if (pp1->pr_type == pp->pr_type &&
		    pp1->pr_protocol == pp->pr_protocol) {
			return EEXIST;
		}
	}

	attach_proto(pp, dp);
	if (doinit) {
		net_init_proto(pp, dp);
	}

	return 0;
}

void
net_init_proto(struct protosw *pp, struct domain *dp)
{
	/*
	 * This could be called as part of initializing the domain,
	 * and thus DOM_INITIALIZED may not be set (yet).  The protocol
	 * must have been attached via net_addr_protosw() by now.
	 */
	domain_proto_mtx_lock_assert_held();
	VERIFY(pp->pr_flags & PR_ATTACHED);

	init_proto(pp, dp);
}

/*
 * Exported (private) routine, indirection of net_add_proto.
 */
int
net_add_proto_old(struct protosw_old *opp, struct domain_old *odp)
{
	struct pr_usrreqs_old *opru;
	struct pr_usrreqs *pru __single = NULL;
	struct protosw *pp __single = NULL, *pp1;
	int error = 0;
	struct domain *dp;
	domain_guard_t guard __single;

	/*
	 * This could be called as part of initializing the domain,
	 * and thus DOM_INITIALIZED may not be set (yet).
	 */
	guard = domain_guard_deploy();

	/* Make sure the domain has been added via net_add_domain */
	TAILQ_FOREACH(dp, &domains, dom_entry) {
		if (!(dp->dom_flags & DOM_OLD)) {
			continue;
		}
		if (dp->dom_old == odp) {
			break;
		}
	}
	if (dp == NULL) {
		error = EINVAL;
		goto done;
	}

	TAILQ_FOREACH(pp1, &dp->dom_protosw, pr_entry) {
		if (pp1->pr_type == opp->pr_type &&
		    pp1->pr_protocol == opp->pr_protocol) {
			error = EEXIST;
			goto done;
		}
	}

	if ((opru = opp->pr_usrreqs) == NULL) {
		panic("%s: domain (%d,%s), proto %d has no usrreqs!",
		    __func__, odp->dom_family, odp->dom_name, opp->pr_protocol);
		/* NOTREACHED */
	}

	pru = kalloc_type(struct pr_usrreqs, Z_WAITOK | Z_ZERO | Z_NOFAIL);

	pru->pru_flags          = PRUF_OLD;
	pru->pru_abort          = opru->pru_abort;
	pru->pru_accept         = opru->pru_accept;
	pru->pru_attach         = opru->pru_attach;
	pru->pru_bind           = opru->pru_bind;
	pru->pru_connect        = opru->pru_connect;
	pru->pru_connect2       = opru->pru_connect2;
	pru->pru_control        = opru->pru_control;
	pru->pru_detach         = opru->pru_detach;
	pru->pru_disconnect     = opru->pru_disconnect;
	pru->pru_listen         = opru->pru_listen;
	pru->pru_peeraddr       = opru->pru_peeraddr;
	pru->pru_rcvd           = opru->pru_rcvd;
	pru->pru_rcvoob         = opru->pru_rcvoob;
	pru->pru_send           = opru->pru_send;
	pru->pru_sense          = opru->pru_sense;
	pru->pru_shutdown       = opru->pru_shutdown;
	pru->pru_sockaddr       = opru->pru_sockaddr;
	pru->pru_sosend         = opru->pru_sosend;
	pru->pru_soreceive      = opru->pru_soreceive;
	pru->pru_sopoll         = opru->pru_sopoll;

	pp = kalloc_type(struct protosw, Z_WAITOK | Z_ZERO | Z_NOFAIL);

	/*
	 * Protocol fast and slow timers are now deprecated.
	 */
	if (opp->pr_unused != NULL) {
		printf("%s: domain (%d,%s), proto %d: pr_fasttimo is "
		    "deprecated and won't be called\n", __func__,
		    odp->dom_family, odp->dom_name, opp->pr_protocol);
	}
	if (opp->pr_unused2 != NULL) {
		printf("%s: domain (%d,%s), proto %d: pr_slowtimo is "
		    "deprecated and won't be called\n", __func__,
		    odp->dom_family, odp->dom_name, opp->pr_protocol);
	}

	/* Copy everything but pr_init, pr_next, pr_domain, pr_protosw */
	pp->pr_type             = opp->pr_type;
	pp->pr_protocol         = opp->pr_protocol;
	pp->pr_flags            = (opp->pr_flags & PRF_USERFLAGS) | PR_OLD;
	pp->pr_input            = opp->pr_input;
	pp->pr_output           = opp->pr_output;
	pp->pr_ctlinput         = opp->pr_ctlinput;
	pp->pr_ctloutput        = opp->pr_ctloutput;
	pp->pr_usrreqs          = pru;
	pp->pr_init             = pr_init_old;
	pp->pr_drain            = opp->pr_drain;
	pp->pr_sysctl           = opp->pr_sysctl;
	pp->pr_lock             = opp->pr_lock;
	pp->pr_unlock           = opp->pr_unlock;
	pp->pr_getlock          = opp->pr_getlock;
	pp->pr_old              = opp;

	/* attach as well as initialize */
	attach_proto(pp, dp);
	net_init_proto(pp, dp);
done:
	if (error != 0) {
		printf("%s: domain (%d,%s), proto %d: failed to attach, "
		    "error %d\n", __func__, odp->dom_family,
		    odp->dom_name, opp->pr_protocol, error);

		kfree_type(struct pr_usrreqs, pru);
		kfree_type(struct protosw, pp);
	}

	domain_guard_release(guard);
	return error;
}

/*
 * Internal routine, not exported.
 *
 * net_del_proto - remove a protosw from a domain's protosw chain.
 * Search the protosw chain for the element with matching data.
 * Then unlink and return.
 *
 * NOTE: Caller must have acquired domain_proto_mtx
 */
int
net_del_proto(int type, int protocol, struct domain *dp)
{
	struct protosw *pp __single;

	/*
	 * This could be called as part of initializing the domain,
	 * and thus DOM_INITIALIZED may not be set (yet).
	 */
	domain_proto_mtx_lock_assert_held();

	TAILQ_FOREACH(pp, &dp->dom_protosw, pr_entry) {
		if (pp->pr_type == type && pp->pr_protocol == protocol) {
			break;
		}
	}
	if (pp == NULL) {
		return ENXIO;
	}

	detach_proto(pp, dp);
	if (pp->pr_usrreqs->pru_flags & PRUF_OLD) {
		kfree_type(struct pr_usrreqs, pp->pr_usrreqs);
	}
	if (pp->pr_flags & PR_OLD) {
		kfree_type(struct protosw, pp);
	}

	return 0;
}

/*
 * Exported (private) routine, indirection of net_del_proto.
 */
int
net_del_proto_old(int type, int protocol, struct domain_old *odp)
{
	int error = 0;
	struct protosw *pp __single;
	struct domain *dp;
	domain_guard_t guard __single;

	/*
	 * This could be called as part of initializing the domain,
	 * and thus DOM_INITIALIZED may not be set (yet).
	 */
	guard = domain_guard_deploy();

	/* Make sure the domain has been added via net_add_domain */
	TAILQ_FOREACH(dp, &domains, dom_entry) {
		if (!(dp->dom_flags & DOM_OLD)) {
			continue;
		}
		if (dp->dom_old == odp) {
			break;
		}
	}
	if (dp == NULL) {
		error = ENXIO;
		goto done;
	}

	TAILQ_FOREACH(pp, &dp->dom_protosw, pr_entry) {
		if (pp->pr_type == type && pp->pr_protocol == protocol) {
			break;
		}
	}
	if (pp == NULL) {
		error = ENXIO;
		goto done;
	}
	detach_proto(pp, dp);
	if (pp->pr_usrreqs->pru_flags & PRUF_OLD) {
		kfree_type(struct pr_usrreqs, pp->pr_usrreqs);
	}
	if (pp->pr_flags & PR_OLD) {
		kfree_type(struct protosw, pp);
	}

done:
	domain_guard_release(guard);
	return error;
}

static void
domain_sched_timeout(void)
{
	LCK_MTX_ASSERT(&domain_timeout_mtx, LCK_MTX_ASSERT_OWNED);

	if (!domain_timeout_run && domain_draining) {
		domain_timeout_run = TRUE;
		timeout(domain_timeout, NULL, hz);
	}
}

void
net_drain_domains(void)
{
	lck_mtx_lock(&domain_timeout_mtx);
	domain_draining = TRUE;
	domain_sched_timeout();
	lck_mtx_unlock(&domain_timeout_mtx);
}

extern struct domain inet6domain_s;
#if IPSEC
extern struct domain keydomain_s;
#endif

extern struct domain routedomain_s, ndrvdomain_s, inetdomain_s;
extern struct domain systemdomain_s, localdomain_s;
extern struct domain vsockdomain_s;

#if MULTIPATH
extern struct domain mpdomain_s;
#endif /* MULTIPATH */

static void
domain_timeout(void *arg)
{
#pragma unused(arg)
	struct protosw *pp;
	struct domain *dp;
	domain_guard_t guard __single;

	lck_mtx_lock(&domain_timeout_mtx);
	if (domain_draining) {
		domain_draining = FALSE;
		lck_mtx_unlock(&domain_timeout_mtx);

		guard = domain_guard_deploy();
		TAILQ_FOREACH(dp, &domains, dom_entry) {
			TAILQ_FOREACH(pp, &dp->dom_protosw, pr_entry) {
				if (pp->pr_drain != NULL) {
					(*pp->pr_drain)();
				}
			}
		}
		domain_guard_release(guard);

		lck_mtx_lock(&domain_timeout_mtx);
	}

	/* re-arm the timer if there's work to do */
	domain_timeout_run = FALSE;
	domain_sched_timeout();
	lck_mtx_unlock(&domain_timeout_mtx);
}

void
domaininit(void)
{
	struct domain *dp;
	domain_guard_t guard __single;

	eventhandler_lists_ctxt_init(&protoctl_evhdlr_ctxt);

	guard = domain_guard_deploy();
	/*
	 * Add all the static domains to the domains list.  route domain
	 * gets added and initialized last, since we need it to attach
	 * rt_tables[] to everything that's already there.  This also
	 * means that domains added after this point won't get their
	 * dom_rtattach() called on rt_tables[].
	 */
	attach_domain(&inetdomain_s);
	attach_domain(&inet6domain_s);
#if MULTIPATH
	attach_domain(&mpdomain_s);
#endif /* MULTIPATH */
	attach_domain(&systemdomain_s);
	attach_domain(&localdomain_s);
#if IPSEC
	attach_domain(&keydomain_s);
#endif /* IPSEC */
	attach_domain(&ndrvdomain_s);
	attach_domain(&vsockdomain_s);
	attach_domain(&routedomain_s);  /* must be last domain */

	/*
	 * Now ask them all to init (XXX including the routing domain,
	 * see above)
	 */
	TAILQ_FOREACH(dp, &domains, dom_entry)
	init_domain(dp);

	domain_guard_release(guard);
}

static __inline__ struct domain *
pffinddomain_locked(int pf)
{
	struct domain *dp;

	domain_proto_mtx_lock_assert_held();

	TAILQ_FOREACH(dp, &domains, dom_entry) {
		if (dp->dom_family == pf) {
			break;
		}
	}
	return dp;
}

struct protosw *
pffindtype(int family, int type)
{
	struct protosw *pp = NULL;
	struct domain *dp;
	domain_guard_t guard __single;

	guard = domain_guard_deploy();
	if ((dp = pffinddomain_locked(family)) == NULL) {
		goto done;
	}

	TAILQ_FOREACH(pp, &dp->dom_protosw, pr_entry) {
		if (pp->pr_type != 0 && pp->pr_type == type) {
			goto done;
		}
	}
done:
	domain_guard_release(guard);
	return pp;
}

/*
 * Internal routine, not exported.
 */
struct domain *
pffinddomain(int pf)
{
	struct domain *dp;
	domain_guard_t guard __single;

	guard = domain_guard_deploy();
	dp = pffinddomain_locked(pf);
	domain_guard_release(guard);
	return dp;
}

/*
 * Exported (private) routine, indirection of pffinddomain.
 */
struct domain_old *
pffinddomain_old(int pf)
{
	struct domain_old *odp = NULL;
	struct domain *dp;
	domain_guard_t guard __single;

	guard = domain_guard_deploy();
	if ((dp = pffinddomain_locked(pf)) != NULL && (dp->dom_flags & DOM_OLD)) {
		odp = dp->dom_old;
	}
	domain_guard_release(guard);
	return odp;
}

/*
 * Internal routine, not exported.
 */
struct protosw *
pffindproto(int family, int protocol, int type)
{
	struct protosw *pp;
	domain_guard_t guard __single;

	guard = domain_guard_deploy();
	pp = pffindproto_locked(family, protocol, type);
	domain_guard_release(guard);
	return pp;
}

struct protosw *
pffindproto_locked(int family, int protocol, int type)
{
	struct protosw *maybe = NULL;
	struct protosw *pp;
	struct domain *dp;

	domain_proto_mtx_lock_assert_held();

	if (family == 0) {
		return 0;
	}

	dp = pffinddomain_locked(family);
	if (dp == NULL) {
		return NULL;
	}

	TAILQ_FOREACH(pp, &dp->dom_protosw, pr_entry) {
		if ((pp->pr_protocol == protocol) && (pp->pr_type == type)) {
			return pp;
		}

		if (type == SOCK_RAW && pp->pr_type == SOCK_RAW &&
		    pp->pr_protocol == 0 && maybe == NULL) {
			maybe = pp;
		}
	}
	return maybe;
}

/*
 * Exported (private) routine, indirection of pffindproto.
 */
struct protosw_old *
pffindproto_old(int family, int protocol, int type)
{
	struct protosw_old *opr = NULL;
	struct protosw *pp;
	domain_guard_t guard __single;

	guard = domain_guard_deploy();
	if ((pp = pffindproto_locked(family, protocol, type)) != NULL &&
	    (pp->pr_flags & PR_OLD)) {
		opr = pp->pr_old;
	}
	domain_guard_release(guard);
	return opr;
}

static struct protosw *
pffindprotonotype_locked(int family, int protocol, int type)
{
#pragma unused(type)
	struct domain *dp;
	struct protosw *pp;

	domain_proto_mtx_lock_assert_held();

	if (family == 0) {
		return 0;
	}

	dp = pffinddomain_locked(family);
	if (dp == NULL) {
		return NULL;
	}

	TAILQ_FOREACH(pp, &dp->dom_protosw, pr_entry) {
		if (pp->pr_protocol == protocol) {
			return pp;
		}
	}
	return NULL;
}

struct protosw *
pffindprotonotype(int family, int protocol)
{
	struct protosw *pp;
	domain_guard_t guard __single;

	if (protocol == 0) {
		return NULL;
	}

	guard = domain_guard_deploy();
	pp = pffindprotonotype_locked(family, protocol, 0);
	domain_guard_release(guard);
	return pp;
}

void
pfctlinput(int cmd, struct sockaddr *sa)
{
	pfctlinput2(cmd, sa, NULL);
}

void
pfctlinput2(int cmd, struct sockaddr *sa, void *ctlparam)
{
	struct domain *dp;
	struct protosw *pp;
	domain_guard_t guard __single;

	if (sa == NULL) {
		return;
	}

	guard = domain_guard_deploy();
	TAILQ_FOREACH(dp, &domains, dom_entry) {
		TAILQ_FOREACH(pp, &dp->dom_protosw, pr_entry) {
			if (pp->pr_ctlinput != NULL) {
				(*pp->pr_ctlinput)(cmd, sa, ctlparam, NULL);
			}
		}
	}
	domain_guard_release(guard);
}

void
net_update_uptime_with_time(const struct timeval *tvp)
{
	uint64_t tmp;
	uint64_t seconds = tvp->tv_sec;;
	uint64_t milliseconds = ((uint64_t)tvp->tv_sec * 1000) + ((uint64_t)tvp->tv_usec / 1000);
	uint64_t microseconds = ((uint64_t)tvp->tv_sec * USEC_PER_SEC) + (uint64_t)tvp->tv_usec;

	/*
	 * Round up the timer to the nearest integer value because otherwise
	 * we might setup networking timers that are off by almost 1 second.
	 */
	if (tvp->tv_usec > 500000) {
		seconds++;
	}

	tmp = os_atomic_load(&_net_uptime, relaxed);
	if (tmp < seconds) {
		os_atomic_cmpxchg(&_net_uptime, tmp, seconds, relaxed);

		/*
		 * No loop needed. If we are racing with another thread, let's give
		 * the other one the priority.
		 */
	}

	/* update milliseconds variant */
	tmp = os_atomic_load(&_net_uptime_ms, relaxed);
	if (tmp < milliseconds) {
		os_atomic_cmpxchg(&_net_uptime_ms, tmp, milliseconds, relaxed);
	}

	/* update microseconds variant */
	tmp = os_atomic_load(&_net_uptime_us, relaxed);
	if (tmp < microseconds) {
		os_atomic_cmpxchg(&_net_uptime_us, tmp, microseconds, relaxed);
	}
}

void
net_update_uptime(void)
{
	struct timeval tv;

	microuptime(&tv);

	net_update_uptime_with_time(&tv);
}

/*
 * Convert our uin64_t net_uptime to a struct timeval.
 */
void
net_uptime2timeval(struct timeval *tv)
{
	if (tv == NULL) {
		return;
	}

	tv->tv_usec = 0;
	tv->tv_sec = (time_t)net_uptime();
}

/*
 * An alternative way to obtain the coarse-grained uptime (in seconds)
 * for networking code which do not require high-precision timestamp,
 * as this is significantly cheaper than microuptime().
 */
uint64_t
net_uptime(void)
{
	if (_net_uptime == 0) {
		net_update_uptime();
	}

	return _net_uptime;
}

uint64_t
net_uptime_ms(void)
{
	if (_net_uptime_ms == 0) {
		net_update_uptime();
	}

	return _net_uptime_ms;
}

uint64_t
net_uptime_us(void)
{
	if (_net_uptime_us == 0) {
		net_update_uptime();
	}

	return _net_uptime_us;
}

void
domain_proto_mtx_lock_assert_held(void)
{
	LCK_MTX_ASSERT(&domain_proto_mtx, LCK_MTX_ASSERT_OWNED);
}

void
domain_proto_mtx_lock_assert_notheld(void)
{
	LCK_MTX_ASSERT(&domain_proto_mtx, LCK_MTX_ASSERT_NOTOWNED);
}

domain_guard_t
domain_guard_deploy(void)
{
	net_thread_marks_t marks __single;

	marks = net_thread_marks_push(NET_THREAD_HELD_DOMAIN);
	if (marks != net_thread_marks_none) {
		LCK_MTX_ASSERT(&domain_proto_mtx, LCK_MTX_ASSERT_NOTOWNED);
		lck_mtx_lock(&domain_proto_mtx);
	} else {
		LCK_MTX_ASSERT(&domain_proto_mtx, LCK_MTX_ASSERT_OWNED);
	}

	return (domain_guard_t)(const void*)marks;
}

void
domain_guard_release(domain_guard_t guard)
{
	net_thread_marks_t marks __single = (net_thread_marks_t)(const void*)guard;

	if (marks != net_thread_marks_none) {
		LCK_MTX_ASSERT(&domain_proto_mtx, LCK_MTX_ASSERT_OWNED);
		lck_mtx_unlock(&domain_proto_mtx);
		net_thread_marks_pop(marks);
	} else {
		LCK_MTX_ASSERT(&domain_proto_mtx, LCK_MTX_ASSERT_NOTOWNED);
	}
}

domain_unguard_t
domain_unguard_deploy(void)
{
	net_thread_marks_t marks __single;

	marks = net_thread_unmarks_push(NET_THREAD_HELD_DOMAIN);
	if (marks != net_thread_marks_none) {
		LCK_MTX_ASSERT(&domain_proto_mtx, LCK_MTX_ASSERT_OWNED);
		lck_mtx_unlock(&domain_proto_mtx);
	} else {
		LCK_MTX_ASSERT(&domain_proto_mtx, LCK_MTX_ASSERT_NOTOWNED);
	}

	return (domain_unguard_t)(const void*)marks;
}

void
domain_unguard_release(domain_unguard_t unguard)
{
	net_thread_marks_t marks __single = (net_thread_marks_t)(const void*)unguard;

	if (marks != net_thread_marks_none) {
		LCK_MTX_ASSERT(&domain_proto_mtx, LCK_MTX_ASSERT_NOTOWNED);
		lck_mtx_lock(&domain_proto_mtx);
		net_thread_unmarks_pop(marks);
	} else {
		LCK_MTX_ASSERT(&domain_proto_mtx, LCK_MTX_ASSERT_OWNED);
	}
}

#if SKYWALK
/* The following is used to enqueue work items for interface events */
struct protoctl_event {
	struct ifnet *ifp;
	union sockaddr_in_4_6 laddr;
	union sockaddr_in_4_6 raddr;
	uint32_t protoctl_event_code;
	struct protoctl_ev_val val;
	uint16_t lport;
	uint16_t rport;
	uint8_t protocol;
};

struct protoctl_event_nwk_wq_entry {
	struct nwk_wq_entry nwk_wqe;
	struct protoctl_event protoctl_ev_arg;
};

static void
protoctl_event_callback(struct nwk_wq_entry *nwk_item)
{
	struct protoctl_event_nwk_wq_entry *p_ev __single = NULL;

	p_ev = __unsafe_forge_single(struct protoctl_event_nwk_wq_entry *,
	    __container_of(nwk_item, struct protoctl_event_nwk_wq_entry, nwk_wqe));

	/* Call this before we walk the tree */
	EVENTHANDLER_INVOKE(&protoctl_evhdlr_ctxt, protoctl_event,
	    p_ev->protoctl_ev_arg.ifp, SA(&p_ev->protoctl_ev_arg.laddr),
	    SA(&p_ev->protoctl_ev_arg.raddr),
	    p_ev->protoctl_ev_arg.lport, p_ev->protoctl_ev_arg.rport,
	    p_ev->protoctl_ev_arg.protocol, p_ev->protoctl_ev_arg.protoctl_event_code,
	    &p_ev->protoctl_ev_arg.val);

	kfree_type(struct protoctl_event_nwk_wq_entry, p_ev);
}

/* XXX Some PRC events needs extra verification like sequence number checking */
void
protoctl_event_enqueue_nwk_wq_entry(struct ifnet *ifp, struct sockaddr *p_laddr,
    struct sockaddr *p_raddr, uint16_t lport, uint16_t rport, uint8_t protocol,
    uint32_t protoctl_event_code, struct protoctl_ev_val *p_protoctl_ev_val)
{
	struct protoctl_event_nwk_wq_entry *p_protoctl_ev = NULL;

	evhlog(debug, "%s: eventhandler enqueuing event of type=protoctl_event event_code=%d",
	    __func__, protocol);

	p_protoctl_ev = kalloc_type(struct protoctl_event_nwk_wq_entry,
	    Z_WAITOK | Z_ZERO | Z_NOFAIL);

	p_protoctl_ev->protoctl_ev_arg.ifp = ifp;

	if (p_laddr != NULL) {
		VERIFY(p_laddr->sa_len <= sizeof(p_protoctl_ev->protoctl_ev_arg.laddr));
		struct sockaddr_in6 *dst __single = &p_protoctl_ev->protoctl_ev_arg.laddr.sin6;
		SOCKADDR_COPY(SIN6(p_laddr), dst, p_laddr->sa_len);
	}

	if (p_raddr != NULL) {
		VERIFY(p_raddr->sa_len <= sizeof(p_protoctl_ev->protoctl_ev_arg.raddr));
		struct sockaddr_in6 *dst __single = &p_protoctl_ev->protoctl_ev_arg.raddr.sin6;
		SOCKADDR_COPY(SIN6(p_raddr), dst, p_raddr->sa_len);
	}

	p_protoctl_ev->protoctl_ev_arg.lport = lport;
	p_protoctl_ev->protoctl_ev_arg.rport = rport;
	p_protoctl_ev->protoctl_ev_arg.protocol = protocol;
	p_protoctl_ev->protoctl_ev_arg.protoctl_event_code = protoctl_event_code;

	if (p_protoctl_ev_val != NULL) {
		bcopy(p_protoctl_ev_val, &(p_protoctl_ev->protoctl_ev_arg.val),
		    sizeof(*p_protoctl_ev_val));
	}
	p_protoctl_ev->nwk_wqe.func = protoctl_event_callback;

	nwk_wq_enqueue(&p_protoctl_ev->nwk_wqe);
}
#endif /* SKYWALK */

#if (DEVELOPMENT || DEBUG)

static int
sysctl_do_drain_domains SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)
	int error;
	int dummy = 0;

	error = sysctl_handle_int(oidp, &dummy, 0, req);
	if (error || req->newptr == USER_ADDR_NULL) {
		return error;
	}

	net_drain_domains();

	return 0;
}

#endif /* DEVELOPMENT || DEBUG */
