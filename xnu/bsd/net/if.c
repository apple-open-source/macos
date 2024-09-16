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
 * Copyright (c) 1980, 1986, 1993
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
 *	@(#)if.c	8.3 (Berkeley) 1/4/94
 * $FreeBSD: src/sys/net/if.c,v 1.85.2.9 2001/07/24 19:10:17 brooks Exp $
 */
/*
 * NOTICE: This file was modified by SPARTA, Inc. in 2006 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 */

#include <kern/locks.h>

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/sockio.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/mcache.h>
#include <sys/kauth.h>
#include <sys/priv.h>
#include <kern/zalloc.h>
#include <mach/boolean.h>

#include <machine/endian.h>

#include <pexpert/pexpert.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/if_ppp.h>
#include <net/ethernet.h>
#include <net/network_agent.h>
#include <net/pktsched/pktsched_netem.h>
#include <net/radix.h>
#include <net/route.h>
#include <net/dlil.h>
#include <net/nwk_wq.h>

#include <sys/domain.h>
#include <libkern/OSAtomic.h>

#if INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_tclass.h>
#include <netinet/ip_var.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet6/in6_var.h>
#include <netinet6/in6_ifattach.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#endif /* INET */

#if SKYWALK
#include <skywalk/nexus/netif/nx_netif.h>
#endif /* SKYWALK */

#include <net/sockaddr_utils.h>

#include <os/log.h>

#include <IOKit/IOBSD.h>

/*
 * System initialization
 */

extern const char *proc_name_address(void *);

/* Lock group and attribute for ifaddr lock */
LCK_ATTR_DECLARE(ifa_mtx_attr, 0, 0);
LCK_GRP_DECLARE(ifa_mtx_grp, "ifaddr");

static int ifioctl_ifreq(struct socket *, u_long, struct ifreq *,
    struct proc *);
static int ifioctl_ifconf(u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)));
static int ifioctl_ifclone(u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)));
static int ifioctl_iforder(u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)));
static int ifioctl_ifdesc(struct ifnet *, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)), struct proc *);
static int ifioctl_linkparams(struct ifnet *, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)), struct proc *);
static int ifioctl_qstats(struct ifnet *, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)));
static int ifioctl_throttle(struct ifnet *, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)), struct proc *);
static int ifioctl_netsignature(struct ifnet *, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)));
static int ifconf(u_long cmd, user_addr_t ifrp, int *ret_space);
__private_extern__ void link_rtrequest(int, struct rtentry *, struct sockaddr *);
void if_rtproto_del(struct ifnet *ifp, int protocol);

static int if_addmulti_common(struct ifnet *, const struct sockaddr *,
    struct ifmultiaddr **, int);
static int if_delmulti_common(struct ifmultiaddr *, struct ifnet *,
    const struct sockaddr *, int);
static struct ifnet *ifunit_common(const char *, boolean_t);

static int if_rtmtu(struct radix_node *, void *);
static void if_rtmtu_update(struct ifnet *);

static int if_clone_list(int, int *, user_addr_t);

MALLOC_DEFINE(M_IFADDR, "ifaddr", "interface address");

struct  ifnethead ifnet_head = TAILQ_HEAD_INITIALIZER(ifnet_head);

/* ifnet_ordered_head and if_ordered_count are protected by the ifnet_head lock */
struct  ifnethead ifnet_ordered_head = TAILQ_HEAD_INITIALIZER(ifnet_ordered_head);
static  u_int32_t if_ordered_count = 0;

static int      if_cloners_count;
LIST_HEAD(, if_clone) if_cloners = LIST_HEAD_INITIALIZER(if_cloners);

static struct ifaddr *ifa_ifwithnet_common(const struct sockaddr *,
    unsigned int);
static void if_attach_ifa_common(struct ifnet *, struct ifaddr *, int);
static void if_detach_ifa_common(struct ifnet *, struct ifaddr *, int);

static void if_attach_ifma(struct ifnet *, struct ifmultiaddr *, int);
static int if_detach_ifma(struct ifnet *, struct ifmultiaddr *, int);

static struct ifmultiaddr *ifma_alloc(zalloc_flags_t);
static void ifma_free(struct ifmultiaddr *);
static void ifma_trace(struct ifmultiaddr *, int);

#if DEBUG
static TUNABLE(bool, ifma_debug, "ifma_debug", true); /* debugging (enabled) */
#else
static TUNABLE(bool, ifma_debug, "ifma_debug", false); /* debugging (disabled) */
#endif /* !DEBUG */

#if DEBUG
ZONE_DECLARE(ifma_zone, struct ifmultiaddr_dbg);
#else
ZONE_DECLARE(ifma_zone, struct ifmultiaddr);
#endif /* !DEBUG */
#define IFMA_ZONE_NAME "ifmultiaddr"    /* zone name */
zone_t ifma_zone = {0};                 /* zone for *ifmultiaddr */

#define IFMA_TRACE_HIST_SIZE    32      /* size of trace history */

/* For gdb */
__private_extern__ unsigned int ifma_trace_hist_size = IFMA_TRACE_HIST_SIZE;

struct ifmultiaddr_dbg {
	struct ifmultiaddr      ifma;                   /* ifmultiaddr */
	u_int16_t               ifma_refhold_cnt;       /* # of ref */
	u_int16_t               ifma_refrele_cnt;       /* # of rele */
	/*
	 * Circular lists of ifa_addref and ifa_remref callers.
	 */
	ctrace_t                ifma_refhold[IFMA_TRACE_HIST_SIZE];
	ctrace_t                ifma_refrele[IFMA_TRACE_HIST_SIZE];
	/*
	 * Trash list linkage
	 */
	TAILQ_ENTRY(ifmultiaddr_dbg) ifma_trash_link;
};

/* List of trash ifmultiaddr entries protected by ifma_trash_lock */
static TAILQ_HEAD(, ifmultiaddr_dbg) ifma_trash_head;
static LCK_MTX_DECLARE_ATTR(ifma_trash_lock, &ifa_mtx_grp, &ifa_mtx_attr);

/*
 * XXX: declare here to avoid to include many inet6 related files..
 * should be more generalized?
 */
extern void     nd6_setmtu(struct ifnet *);

SYSCTL_NODE(_net, PF_LINK, link, CTLFLAG_RW | CTLFLAG_LOCKED, 0, "Link layers");
SYSCTL_NODE(_net_link, 0, generic, CTLFLAG_RW | CTLFLAG_LOCKED, 0,
    "Generic link-management");

SYSCTL_DECL(_net_link_generic_system);

static uint32_t if_verbose = 0;
SYSCTL_INT(_net_link_generic_system, OID_AUTO, if_verbose,
    CTLFLAG_RW | CTLFLAG_LOCKED, &if_verbose, 0, "");

#if (DEBUG || DEVELOPMENT)
static uint32_t default_tcp_kao_max = 0;
SYSCTL_INT(_net_link_generic_system, OID_AUTO, default_tcp_kao_max,
    CTLFLAG_RW | CTLFLAG_LOCKED, &default_tcp_kao_max, 0, "");
#else
static const uint32_t default_tcp_kao_max = 0;
#endif /* (DEBUG || DEVELOPMENT) */

u_int32_t companion_link_sock_buffer_limit = 0;

static int
sysctl_set_companion_link_sock_buf_limit SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)
	int error, tmp = companion_link_sock_buffer_limit;
	error = sysctl_handle_int(oidp, &tmp, 0, req);
	if (tmp < 0) {
		return EINVAL;
	}
	if ((error = priv_check_cred(kauth_cred_get(),
	    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
		return error;
	}

	u_int32_t new_limit = tmp;
	if (new_limit == companion_link_sock_buffer_limit) {
		return 0;
	}

	bool recover = new_limit == 0 ? true : false;
	if (recover) {
		error = inp_recover_companion_link(&tcbinfo);
	} else {
		error = inp_limit_companion_link(&tcbinfo, new_limit);
	}
	if (!error) {
		companion_link_sock_buffer_limit = new_limit;
	}
	return error;
}

SYSCTL_PROC(_net_link_generic_system, OID_AUTO, companion_sndbuf_limit,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED | CTLFLAG_ANYBODY,
    &companion_link_sock_buffer_limit, 0, sysctl_set_companion_link_sock_buf_limit,
    "I", "set sock send buffer limit of connections using companion links");


TUNABLE(bool, intcoproc_unrestricted, "intcoproc_unrestricted", false);

SYSCTL_NODE(_net_link_generic_system, OID_AUTO, management,
    CTLFLAG_RW | CTLFLAG_LOCKED, 0, "management interface");

TUNABLE_WRITEABLE(int, if_management_verbose, "management_data_unrestricted", 0);

SYSCTL_INT(_net_link_generic_system_management, OID_AUTO, verbose,
    CTLFLAG_RW | CTLFLAG_LOCKED, &if_management_verbose, 0, "");

/*
 * boot-args to disable entitlement check for data transfer on management interface
 */
TUNABLE_DEV_WRITEABLE(bool, management_data_unrestricted, "management_data_unrestricted", false);

#if DEBUG || DEVELOPMENT
#define MANAGEMENT_CTLFLAG_ACCESS CTLFLAG_RW
#else
#define MANAGEMENT_CTLFLAG_ACCESS CTLFLAG_RD
#endif

static int
sysctl_management_data_unrestricted SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int val = management_data_unrestricted;

	int error = sysctl_handle_int(oidp, &val, 0, req);
#if DEBUG || DEVELOPMENT
	if (error == 0 && req->newptr != USER_ADDR_NULL) {
		management_data_unrestricted = (val == 0) ? false : true;
		if (if_management_verbose > 0) {
			os_log(OS_LOG_DEFAULT,
			    "sysctl_management_data_unrestricted val %d -> management_data_unrestricted %d",
			    val, management_data_unrestricted);
		}
	}
#endif /* DEBUG || DEVELOPMENT */
	return error;
}

SYSCTL_PROC(_net_link_generic_system_management, OID_AUTO, data_unrestricted,
    CTLTYPE_INT | MANAGEMENT_CTLFLAG_ACCESS | CTLFLAG_LOCKED, 0, 0,
    sysctl_management_data_unrestricted, "I", "");

/*
 * boot-args to disable entitlement restrictions to control management interfaces
 */
TUNABLE_DEV_WRITEABLE(bool, management_control_unrestricted, "management_control_unrestricted", false);

static int
sysctl_management_control_unrestricted SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int val = management_control_unrestricted;

	int error = sysctl_handle_int(oidp, &val, 0, req);
#if DEBUG || DEVELOPMENT
	if (error == 0 && req->newptr != USER_ADDR_NULL) {
		management_control_unrestricted = (val == 0) ? false : true;
		if (if_management_verbose > 0) {
			os_log(OS_LOG_DEFAULT,
			    "sysctl_management_control_unrestricted val %d -> management_control_unrestricted %d",
			    val, management_control_unrestricted);
		}
	}
#endif /* DEBUG || DEVELOPMENT */
	return error;
}

SYSCTL_PROC(_net_link_generic_system_management, OID_AUTO, control_unrestricted,
    CTLTYPE_INT | MANAGEMENT_CTLFLAG_ACCESS | CTLFLAG_LOCKED, 0, 0,
    sysctl_management_control_unrestricted, "I", "");

#undef MANAGEMENT_CTLFLAG_ACCESS

/* The following is set as soon as IFNET_SUBFAMILY_MANAGEMENT is used */
bool if_management_interface_check_needed = false;

/* The following is set when some interface is marked with IFXF_ULTRA_CONSTRAINED */
bool if_ultra_constrained_check_needed = false;

/* Eventhandler context for interface events */
struct eventhandler_lists_ctxt ifnet_evhdlr_ctxt;

void
ifa_init(void)
{
	size_t ifma_size = (ifma_debug == 0) ? sizeof(struct ifmultiaddr) :
	    sizeof(struct ifmultiaddr_dbg);

	ifma_zone = zone_create(IFMA_ZONE_NAME, ifma_size, ZC_NONE);
	TAILQ_INIT(&ifma_trash_head);
}

/*
 * Network interface utility routines.
 *
 * Routines with ifa_ifwith* names take sockaddr *'s as
 * parameters.
 */

int if_index;
int if_indexcount;
uint32_t ifindex2ifnetcount;
struct ifnet **__counted_by_or_null(ifindex2ifnetcount) ifindex2ifnet;
struct ifaddr **__counted_by_or_null(if_indexcount) ifnet_addrs;

__private_extern__ void
if_attach_ifa(struct ifnet *ifp, struct ifaddr *ifa)
{
	if_attach_ifa_common(ifp, ifa, 0);
}

__private_extern__ void
if_attach_link_ifa(struct ifnet *ifp, struct ifaddr *ifa)
{
	if_attach_ifa_common(ifp, ifa, 1);
}

static void
if_attach_ifa_common(struct ifnet *ifp, struct ifaddr *ifa, int link)
{
	ifnet_lock_assert(ifp, IFNET_LCK_ASSERT_EXCLUSIVE);
	IFA_LOCK_ASSERT_HELD(ifa);

	if (ifa->ifa_ifp != ifp) {
		panic("%s: Mismatch ifa_ifp=%p != ifp=%p", __func__,
		    ifa->ifa_ifp, ifp);
		/* NOTREACHED */
	} else if (ifa->ifa_debug & IFD_ATTACHED) {
		panic("%s: Attempt to attach an already attached ifa=%p",
		    __func__, ifa);
		/* NOTREACHED */
	} else if (link && !(ifa->ifa_debug & IFD_LINK)) {
		panic("%s: Unexpected non-link address ifa=%p", __func__, ifa);
		/* NOTREACHED */
	} else if (!link && (ifa->ifa_debug & IFD_LINK)) {
		panic("%s: Unexpected link address ifa=%p", __func__, ifa);
		/* NOTREACHED */
	}
	ifa_addref(ifa);
	ifa->ifa_debug |= IFD_ATTACHED;

	if (link) {
		TAILQ_INSERT_HEAD(&ifp->if_addrhead, ifa, ifa_link);
	} else {
		TAILQ_INSERT_TAIL(&ifp->if_addrhead, ifa, ifa_link);
	}

#if SKYWALK
	SK_NXS_MS_IF_ADDR_GENCNT_INC(ifp);
#endif /* SKYWALK */
}

__private_extern__ void
if_detach_ifa(struct ifnet *ifp, struct ifaddr *ifa)
{
	if_detach_ifa_common(ifp, ifa, 0);
}

__private_extern__ void
if_detach_link_ifa(struct ifnet *ifp, struct ifaddr *ifa)
{
	if_detach_ifa_common(ifp, ifa, 1);
}

static void
if_detach_ifa_common(struct ifnet *ifp, struct ifaddr *ifa, int link)
{
	ifnet_lock_assert(ifp, IFNET_LCK_ASSERT_EXCLUSIVE);
	IFA_LOCK_ASSERT_HELD(ifa);

	if (link && !(ifa->ifa_debug & IFD_LINK)) {
		panic("%s: Unexpected non-link address ifa=%p", __func__, ifa);
		/* NOTREACHED */
	} else if (link && ifa != TAILQ_FIRST(&ifp->if_addrhead)) {
		panic("%s: Link address ifa=%p not first", __func__, ifa);
		/* NOTREACHED */
	} else if (!link && (ifa->ifa_debug & IFD_LINK)) {
		panic("%s: Unexpected link address ifa=%p", __func__, ifa);
		/* NOTREACHED */
	} else if (!(ifa->ifa_debug & IFD_ATTACHED)) {
		panic("%s: Attempt to detach an unattached address ifa=%p",
		    __func__, ifa);
		/* NOTREACHED */
	} else if (ifa->ifa_ifp != ifp) {
		panic("%s: Mismatch ifa_ifp=%p, ifp=%p", __func__,
		    ifa->ifa_ifp, ifp);
		/* NOTREACHED */
	} else if (ifa->ifa_debug & IFD_DEBUG) {
		struct ifaddr *ifa2;
		TAILQ_FOREACH(ifa2, &ifp->if_addrhead, ifa_link) {
			if (ifa2 == ifa) {
				break;
			}
		}
		if (ifa2 != ifa) {
			panic("%s: Attempt to detach a stray address ifa=%p",
			    __func__, ifa);
			/* NOTREACHED */
		}
	}
	TAILQ_REMOVE(&ifp->if_addrhead, ifa, ifa_link);
	ifa->ifa_debug &= ~IFD_ATTACHED;
	ifa_remref(ifa);

#if SKYWALK
	SK_NXS_MS_IF_ADDR_GENCNT_INC(ifp);
#endif /* SKYWALK */
}

#define INITIAL_IF_INDEXLIM     8

/*
 * Function: if_next_index
 * Purpose:
 *   Return the next available interface index.
 *   Grow the ifnet_addrs[] and ifindex2ifnet[] arrays to accomodate the
 *   added entry when necessary.
 *
 * Note:
 *   ifnet_addrs[] is indexed by (if_index - 1), whereas
 *   ifindex2ifnet[] is indexed by ifp->if_index.  That requires us to
 *   always allocate one extra element to hold ifindex2ifnet[0], which
 *   is unused.
 */
int if_next_index(void);

__private_extern__ int
if_next_index(void)
{
	static int      if_indexlim = 0;
	int             new_index;

	/*
	 * Although we are returning an integer,
	 * ifnet's if_index is a uint16_t which means
	 * that's our upper bound.
	 */
	if (if_index >= UINT16_MAX) {
		return -1;
	}
	new_index = ++if_index;
	if (if_index > if_indexlim) {
		unsigned        n;
		int             new_if_indexlim;
		caddr_t         new_ifnet_addrs;
		caddr_t         new_ifindex2ifnet;
		caddr_t         old_ifnet_addrs;
		size_t          old_ifnet_size;

		old_ifnet_addrs = (caddr_t)ifnet_addrs;
		old_ifnet_size = (size_t)(2 * if_indexlim + 1);
		if (ifnet_addrs == NULL) {
			new_if_indexlim = INITIAL_IF_INDEXLIM;
		} else {
			new_if_indexlim = if_indexlim << 1;
		}

		/* allocate space for the larger arrays */
		n = (2 * new_if_indexlim + 1);
		new_ifnet_addrs = (caddr_t)kalloc_type(caddr_t, n, Z_WAITOK | Z_ZERO);
		if (new_ifnet_addrs == NULL) {
			--if_index;
			return -1;
		}

		new_ifindex2ifnet = new_ifnet_addrs + new_if_indexlim * sizeof(caddr_t);
		if (ifnet_addrs != NULL) {
			/* copy the existing data */
			bcopy(ifnet_addrs, new_ifnet_addrs, if_indexlim * sizeof(caddr_t));
			bcopy(ifindex2ifnet, new_ifindex2ifnet, (if_indexlim + 1) * sizeof(caddr_t));
		}

		/* switch to the new tables and size */
		ifnet_addrs = (struct ifaddr **)(void *)new_ifnet_addrs;
		if_indexcount = new_if_indexlim;
		if_indexlim = new_if_indexlim;

		ifindex2ifnet = (struct ifnet **)(void *)new_ifindex2ifnet;
		ifindex2ifnetcount = if_indexlim + 1;

		/* release the old data */
		if (old_ifnet_addrs != NULL) {
			void *old_ifnet_addrs_p = (void *)old_ifnet_addrs;
			kfree_type(caddr_t, old_ifnet_size, old_ifnet_addrs_p);
		}
	}
	return new_index;
}

/*
 * Create a clone network interface.
 */
static int
if_clone_create(char *__counted_by(len) name, uint8_t len, void *params)
{
	struct if_clone *ifc;
	struct ifnet *ifp;
	char *dp;
	int wildcard;
	u_int32_t bytoff, bitoff;
	u_int32_t unit;
	int err;

	ifc = if_clone_lookup(name, len, &unit);
	if (ifc == NULL) {
		return EINVAL;
	}

	if (ifunit(__unsafe_null_terminated_from_indexable(name)) != NULL) {
		return EEXIST;
	}

	bytoff = bitoff = 0;
	wildcard = (unit == UINT32_MAX);
	/*
	 * Find a free unit if none was given.
	 */
	lck_mtx_lock(&ifc->ifc_mutex);
again:
	if (wildcard) {
		while ((bytoff < ifc->ifc_bmlen) &&
		    (ifc->ifc_units[bytoff] == 0xff)) {
			bytoff++;
		}
		if (bytoff >= ifc->ifc_bmlen) {
			lck_mtx_lock(&ifc->ifc_mutex);
			return ENOSPC;
		}
		while ((ifc->ifc_units[bytoff] & (1 << bitoff)) != 0) {
			bitoff++;
		}
		unit = (bytoff << 3) + bitoff;
	}

	if (unit > ifc->ifc_maxunit) {
		lck_mtx_unlock(&ifc->ifc_mutex);
		return ENXIO;
	}

	err = (*ifc->ifc_create)(ifc, unit, params);
	if (err != 0) {
		if (wildcard && err == EBUSY) {
			bitoff++;
			goto again;
		}
		lck_mtx_unlock(&ifc->ifc_mutex);
		return err;
	}

	if (!wildcard) {
		bytoff = unit >> 3;
		bitoff = unit - (bytoff << 3);
	}

	/*
	 * Allocate the unit in the bitmap.
	 */
	KASSERT((ifc->ifc_units[bytoff] & (1 << bitoff)) == 0,
	    ("%s: bit is already set", __func__));
	ifc->ifc_units[bytoff] |= (unsigned char)(1 << bitoff);

	/* In the wildcard case, we need to update the name. */
	if (wildcard) {
		for (dp = (char *)name; *dp != '\0'; dp++) {
			;
		}
		if (snprintf(dp, len - (dp - name), "%d", unit) >
		    len - (dp - name) - 1) {
			/*
			 * This can only be a programmer error and
			 * there's no straightforward way to recover if
			 * it happens.
			 */
			panic("%s: interface name too long", __func__);
			/* NOTREACHED */
		}
	}
	lck_mtx_unlock(&ifc->ifc_mutex);
	ifp = ifunit(__unsafe_null_terminated_from_indexable(name));
	if (ifp != NULL) {
		if_set_eflags(ifp, IFEF_CLONE);
	}
	return 0;
}

/*
 * Destroy a clone network interface.
 */
static int
if_clone_destroy(const char *__counted_by(namelen) name, size_t namelen)
{
	struct if_clone *__single ifc = NULL;
	struct ifnet *__single ifp = NULL;
	int bytoff, bitoff;
	u_int32_t unit;
	int error = 0;

	ifc = if_clone_lookup(name, namelen, &unit);
	if (ifc == NULL) {
		error = EINVAL;
		goto done;
	}

	if (unit < ifc->ifc_minifs) {
		error = EINVAL;
		goto done;
	}

	ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(name));
	if (ifp == NULL) {
		error = ENXIO;
		goto done;
	}
	if ((ifp->if_eflags & IFEF_CLONE) == 0) {
		error = EOPNOTSUPP;
		goto done;
	}
	if (ifc->ifc_destroy == NULL) {
		error = EOPNOTSUPP;
		goto done;
	}

	lck_mtx_lock(&ifc->ifc_mutex);
	error = (*ifc->ifc_destroy)(ifp);

	if (error) {
		lck_mtx_unlock(&ifc->ifc_mutex);
		goto done;
	}

	/* Compute offset in the bitmap and deallocate the unit. */
	bytoff = unit >> 3;
	bitoff = unit - (bytoff << 3);
	KASSERT((ifc->ifc_units[bytoff] & (1 << bitoff)) != 0,
	    ("%s: bit is already cleared", __func__));
	ifc->ifc_units[bytoff] &= ~(1 << bitoff);
	lck_mtx_unlock(&ifc->ifc_mutex);

done:
	if (ifp != NULL) {
		ifnet_decr_iorefcnt(ifp);
	}
	return error;
}

/*
 * Look up a network interface cloner.
 */

__private_extern__ struct if_clone *
if_clone_lookup(const char *__counted_by(namelen) name, size_t namelen, u_int32_t *unitp)
{
	struct if_clone *ifc;
	const char *cp;
	u_int32_t i;


	LIST_FOREACH(ifc, &if_cloners, ifc_list) {
		size_t c_len = namelen < ifc->ifc_namelen ? namelen : ifc->ifc_namelen;
		if (strbufcmp(ifc->ifc_name, ifc->ifc_namelen, name, c_len) == 0) {
			cp = name + ifc->ifc_namelen;
			goto found_name;
		}
	}

	/* No match. */
	return (struct if_clone *)NULL;

found_name:
	if (*cp == '\0') {
		i = UINT32_MAX;
	} else {
		for (i = 0; *cp != '\0'; cp++) {
			if (*cp < '0' || *cp > '9') {
				/* Bogus unit number. */
				return NULL;
			}
			i = (i * 10) + (*cp - '0');
		}
	}

	if (unitp != NULL) {
		*unitp = i;
	}
	return ifc;
}

/*
 * Register a network interface cloner.
 */
int
if_clone_attach(struct if_clone *ifc)
{
	int bytoff, bitoff;
	int err;
	int len, maxclone;
	u_int32_t unit;
	unsigned char *ifc_units = NULL;

	KASSERT(ifc->ifc_minifs - 1 <= ifc->ifc_maxunit,
	    ("%s: %s requested more units then allowed (%d > %d)",
	    __func__, ifc->ifc_name, ifc->ifc_minifs,
	    ifc->ifc_maxunit + 1));
	/*
	 * Compute bitmap size and allocate it.
	 */
	maxclone = ifc->ifc_maxunit + 1;
	len = maxclone >> 3;
	if ((len << 3) < maxclone) {
		len++;
	}

	ifc_units = (unsigned char *)kalloc_data(len, Z_WAITOK | Z_ZERO);
	if (ifc_units == NULL) {
		return ENOBUFS;
	}
	ifc->ifc_units = ifc_units;
	ifc->ifc_bmlen = len;
	lck_mtx_init(&ifc->ifc_mutex, &ifnet_lock_group, &ifnet_lock_attr);

	LIST_INSERT_HEAD(&if_cloners, ifc, ifc_list);
	if_cloners_count++;

	for (unit = 0; unit < ifc->ifc_minifs; unit++) {
		err = (*ifc->ifc_create)(ifc, unit, NULL);
		KASSERT(err == 0,
		    ("%s: failed to create required interface %s%d",
		    __func__, ifc->ifc_name, unit));

		/* Allocate the unit in the bitmap. */
		bytoff = unit >> 3;
		bitoff = unit - (bytoff << 3);
		ifc->ifc_units[bytoff] |= (unsigned char)(1 << bitoff);
	}

	return 0;
}

/*
 * Unregister a network interface cloner.
 */
void
if_clone_detach(struct if_clone *ifc)
{
	LIST_REMOVE(ifc, ifc_list);
	kfree_data_sized_by(ifc->ifc_units, ifc->ifc_bmlen);
	lck_mtx_destroy(&ifc->ifc_mutex, &ifnet_lock_group);
	if_cloners_count--;
}

/*
 * Provide list of interface cloners to userspace.
 */
static int
if_clone_list(int count, int *ret_total, user_addr_t dst)
{
	char outbuf[IFNAMSIZ];
	struct if_clone *ifc;
	int error = 0;

	*ret_total = if_cloners_count;
	if (dst == USER_ADDR_NULL) {
		/* Just asking how many there are. */
		return 0;
	}

	if (count < 0) {
		return EINVAL;
	}

	count = (if_cloners_count < count) ? if_cloners_count : count;

	for (ifc = LIST_FIRST(&if_cloners); ifc != NULL && count != 0;
	    ifc = LIST_NEXT(ifc, ifc_list), count--, dst += IFNAMSIZ) {
		bzero(outbuf, sizeof(outbuf));
		strbufcpy(outbuf, IFNAMSIZ, ifc->ifc_name, IFNAMSIZ);
		error = copyout(outbuf, dst, IFNAMSIZ);
		if (error) {
			break;
		}
	}

	return error;
}

u_int32_t
if_functional_type(struct ifnet *ifp, bool exclude_delegate)
{
	u_int32_t ret = IFRTYPE_FUNCTIONAL_UNKNOWN;

	if (ifp != NULL) {
		if (ifp->if_flags & IFF_LOOPBACK) {
			ret = IFRTYPE_FUNCTIONAL_LOOPBACK;
		} else if (IFNET_IS_COMPANION_LINK(ifp)) {
			ret = IFRTYPE_FUNCTIONAL_COMPANIONLINK;
		} else if ((exclude_delegate &&
		    (ifp->if_family == IFNET_FAMILY_ETHERNET &&
		    ifp->if_subfamily == IFNET_SUBFAMILY_WIFI)) ||
		    (!exclude_delegate && IFNET_IS_WIFI(ifp))) {
			if (ifp->if_eflags & IFEF_AWDL) {
				ret = IFRTYPE_FUNCTIONAL_WIFI_AWDL;
			} else {
				ret = IFRTYPE_FUNCTIONAL_WIFI_INFRA;
			}
		} else if ((exclude_delegate &&
		    (ifp->if_type == IFT_CELLULAR ||
		    (ifp->if_family == IFNET_FAMILY_ETHERNET &&
		    ifp->if_subfamily == IFNET_SUBFAMILY_SIMCELL))) ||
		    (!exclude_delegate && IFNET_IS_CELLULAR(ifp))) {
			ret = IFRTYPE_FUNCTIONAL_CELLULAR;
		} else if (IFNET_IS_INTCOPROC(ifp)) {
			ret = IFRTYPE_FUNCTIONAL_INTCOPROC;
		} else if (IFNET_IS_MANAGEMENT(ifp)) {
			ret = IFRTYPE_FUNCTIONAL_MANAGEMENT;
		} else if ((exclude_delegate &&
		    (ifp->if_family == IFNET_FAMILY_ETHERNET ||
		    ifp->if_family == IFNET_FAMILY_BOND ||
		    ifp->if_family == IFNET_FAMILY_VLAN ||
		    ifp->if_family == IFNET_FAMILY_FIREWIRE)) ||
		    (!exclude_delegate && IFNET_IS_WIRED(ifp))) {
			ret = IFRTYPE_FUNCTIONAL_WIRED;
		}
	}

	return ret;
}

u_int32_t
if_peer_egress_functional_type(struct ifnet *ifp, bool exclude_delegate)
{
	u_int32_t ret = IFRTYPE_FUNCTIONAL_UNKNOWN;

	if (ifp != NULL) {
		if (exclude_delegate) {
			if (ifp->peer_egress_functional_type == IFRTYPE_FUNCTIONAL_WIFI_INFRA) {
				ret = IFRTYPE_FUNCTIONAL_WIFI_INFRA;
			} else if (ifp->peer_egress_functional_type == IFRTYPE_FUNCTIONAL_CELLULAR) {
				ret = IFRTYPE_FUNCTIONAL_CELLULAR;
			} else if (ifp->peer_egress_functional_type == IFRTYPE_FUNCTIONAL_WIRED) {
				ret = IFRTYPE_FUNCTIONAL_WIRED;
			}
		} else { // !exclude_delegate
			struct ifnet *if_delegated_ifp = ifp->if_delegated.ifp;
			if (if_delegated_ifp != NULL) {
				if (if_delegated_ifp->peer_egress_functional_type == IFRTYPE_FUNCTIONAL_WIFI_INFRA) {
					ret = IFRTYPE_FUNCTIONAL_WIFI_INFRA;
				} else if (if_delegated_ifp->peer_egress_functional_type == IFRTYPE_FUNCTIONAL_CELLULAR) {
					ret = IFRTYPE_FUNCTIONAL_CELLULAR;
				} else if (if_delegated_ifp->peer_egress_functional_type == IFRTYPE_FUNCTIONAL_WIRED) {
					ret = IFRTYPE_FUNCTIONAL_WIRED;
				}
			}
		}
	}

	return ret;
}

/*
 * Similar to ifa_ifwithaddr, except that this is IPv4 specific
 * and that it matches only the local (not broadcast) address.
 */
__private_extern__ struct in_ifaddr *
ifa_foraddr(unsigned int addr)
{
	return ifa_foraddr_scoped(addr, IFSCOPE_NONE);
}

/*
 * Similar to ifa_foraddr, except with the added interface scope
 * constraint (unless the caller passes in IFSCOPE_NONE in which
 * case there is no scope restriction).
 */
__private_extern__ struct in_ifaddr *
ifa_foraddr_scoped(unsigned int addr, unsigned int scope)
{
	struct in_ifaddr *ia = NULL;

	lck_rw_lock_shared(&in_ifaddr_rwlock);
	TAILQ_FOREACH(ia, INADDR_HASH(addr), ia_hash) {
		IFA_LOCK_SPIN(&ia->ia_ifa);
		if (ia->ia_addr.sin_addr.s_addr == addr &&
		    (scope == IFSCOPE_NONE || ia->ia_ifp->if_index == scope)) {
			ifa_addref(&ia->ia_ifa); /* for caller */
			IFA_UNLOCK(&ia->ia_ifa);
			break;
		}
		IFA_UNLOCK(&ia->ia_ifa);
	}
	lck_rw_done(&in_ifaddr_rwlock);
	return ia;
}

/*
 * Similar to ifa_foraddr, except that this for IPv6.
 */
__private_extern__ struct in6_ifaddr *
ifa_foraddr6(struct in6_addr *addr6)
{
	return ifa_foraddr6_scoped(addr6, IFSCOPE_NONE);
}

__private_extern__ struct in6_ifaddr *
ifa_foraddr6_scoped(struct in6_addr *addr6, unsigned int scope)
{
	struct in6_ifaddr *ia = NULL;

	lck_rw_lock_shared(&in6_ifaddr_rwlock);
	TAILQ_FOREACH(ia, IN6ADDR_HASH(addr6), ia6_hash) {
		IFA_LOCK(&ia->ia_ifa);
		if (IN6_ARE_ADDR_EQUAL(&ia->ia_addr.sin6_addr, addr6) &&
		    (scope == IFSCOPE_NONE || ia->ia_ifp->if_index == scope)) {
			ifa_addref(&ia->ia_ifa); /* for caller */
			IFA_UNLOCK(&ia->ia_ifa);
			break;
		}
		IFA_UNLOCK(&ia->ia_ifa);
	}
	lck_rw_done(&in6_ifaddr_rwlock);

	return ia;
}

/*
 * Return the first (primary) address of a given family on an interface.
 */
__private_extern__ struct ifaddr *
ifa_ifpgetprimary(struct ifnet *ifp, int family)
{
	struct ifaddr *ifa;

	ifnet_lock_shared(ifp);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		IFA_LOCK_SPIN(ifa);
		if (ifa->ifa_addr->sa_family == family) {
			ifa_addref(ifa); /* for caller */
			IFA_UNLOCK(ifa);
			break;
		}
		IFA_UNLOCK(ifa);
	}
	ifnet_lock_done(ifp);

	return ifa;
}

inline boolean_t
sa_equal(const struct sockaddr *sa1, const struct sockaddr *sa2)
{
	if (!sa1 || !sa2) {
		return FALSE;
	}
	if (sa1->sa_len != sa2->sa_len) {
		return FALSE;
	}

	return SOCKADDR_CMP(sa1, sa2, sa1->sa_len) == 0;
}

/*
 * Locate an interface based on a complete address.
 */
struct ifaddr *
ifa_ifwithaddr_locked(const struct sockaddr *addr)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct ifaddr *result = NULL;

	for (ifp = ifnet_head.tqh_first; ifp && !result;
	    ifp = ifp->if_link.tqe_next) {
		ifnet_lock_shared(ifp);
		for (ifa = ifp->if_addrhead.tqh_first; ifa;
		    ifa = ifa->ifa_link.tqe_next) {
			IFA_LOCK_SPIN(ifa);
			if (ifa->ifa_addr->sa_family != addr->sa_family) {
				IFA_UNLOCK(ifa);
				continue;
			}
			if (sa_equal(addr, ifa->ifa_addr)) {
				result = ifa;
				ifa_addref(ifa); /* for caller */
				IFA_UNLOCK(ifa);
				break;
			}
			if ((ifp->if_flags & IFF_BROADCAST) &&
			    ifa->ifa_broadaddr != NULL &&
			    /* IP6 doesn't have broadcast */
			    ifa->ifa_broadaddr->sa_len != 0 &&
			    sa_equal(ifa->ifa_broadaddr, addr)) {
				result = ifa;
				ifa_addref(ifa); /* for caller */
				IFA_UNLOCK(ifa);
				break;
			}
			IFA_UNLOCK(ifa);
		}
		ifnet_lock_done(ifp);
	}

	return result;
}

struct ifaddr *
ifa_ifwithaddr(const struct sockaddr *addr)
{
	struct ifaddr *result = NULL;

	ifnet_head_lock_shared();

	result = ifa_ifwithaddr_locked(addr);

	ifnet_head_done();

	return result;
}
/*
 * Locate the point to point interface with a given destination address.
 */
/*ARGSUSED*/

static struct ifaddr *
ifa_ifwithdstaddr_ifp(const struct sockaddr *addr, struct ifnet * ifp)
{
	struct ifaddr *ifa;
	struct ifaddr *result = NULL;

	if ((ifp->if_flags & IFF_POINTOPOINT) != 0) {
		ifnet_lock_shared(ifp);

		for (ifa = ifp->if_addrhead.tqh_first; ifa;
		    ifa = ifa->ifa_link.tqe_next) {
			IFA_LOCK_SPIN(ifa);
			if (ifa->ifa_addr->sa_family !=
			    addr->sa_family) {
				IFA_UNLOCK(ifa);
				continue;
			}
			if (sa_equal(addr, ifa->ifa_dstaddr)) {
				result = ifa;
				ifa_addref(ifa); /* for caller */
				IFA_UNLOCK(ifa);
				break;
			}
			IFA_UNLOCK(ifa);
		}

		ifnet_lock_done(ifp);
	}
	return result;
}

struct ifaddr *
ifa_ifwithdstaddr(const struct sockaddr *addr)
{
	struct ifnet *ifp;
	struct ifaddr *result = NULL;

	ifnet_head_lock_shared();
	for (ifp = ifnet_head.tqh_first; ifp && !result;
	    ifp = ifp->if_link.tqe_next) {
		result = ifa_ifwithdstaddr_ifp(addr, ifp);
	}
	ifnet_head_done();
	return result;
}

struct ifaddr *
ifa_ifwithdstaddr_scoped(const struct sockaddr *addr, unsigned int ifscope)
{
	struct ifnet *ifp;
	struct ifaddr *result = NULL;

	if (ifscope == IFSCOPE_NONE) {
		return ifa_ifwithdstaddr(addr);
	}
	ifnet_head_lock_shared();
	if (ifscope <= (unsigned int)if_index) {
		ifp = ifindex2ifnet[ifscope];
		if (ifp != NULL) {
			result = ifa_ifwithdstaddr_ifp(addr, ifp);
		}
	}
	ifnet_head_done();
	return result;
}

/*
 * Locate the source address of an interface based on a complete address.
 */
struct ifaddr *
ifa_ifwithaddr_scoped_locked(const struct sockaddr *addr, unsigned int ifscope)
{
	struct ifaddr *result = NULL;
	struct ifnet *ifp;

	if (ifscope == IFSCOPE_NONE) {
		return ifa_ifwithaddr_locked(addr);
	}

	if (ifscope > (unsigned int)if_index) {
		return NULL;
	}

	ifp = ifindex2ifnet[ifscope];
	if (ifp != NULL) {
		struct ifaddr *ifa = NULL;

		/*
		 * This is suboptimal; there should be a better way
		 * to search for a given address of an interface
		 * for any given address family.
		 */
		ifnet_lock_shared(ifp);
		for (ifa = ifp->if_addrhead.tqh_first; ifa != NULL;
		    ifa = ifa->ifa_link.tqe_next) {
			IFA_LOCK_SPIN(ifa);
			if (ifa->ifa_addr->sa_family != addr->sa_family) {
				IFA_UNLOCK(ifa);
				continue;
			}
			if (sa_equal(addr, ifa->ifa_addr)) {
				result = ifa;
				ifa_addref(ifa); /* for caller */
				IFA_UNLOCK(ifa);
				break;
			}
			if ((ifp->if_flags & IFF_BROADCAST) &&
			    ifa->ifa_broadaddr != NULL &&
			    /* IP6 doesn't have broadcast */
			    ifa->ifa_broadaddr->sa_len != 0 &&
			    sa_equal(ifa->ifa_broadaddr, addr)) {
				result = ifa;
				ifa_addref(ifa); /* for caller */
				IFA_UNLOCK(ifa);
				break;
			}
			IFA_UNLOCK(ifa);
		}
		ifnet_lock_done(ifp);
	}

	return result;
}

struct ifaddr *
ifa_ifwithaddr_scoped(const struct sockaddr *addr, unsigned int ifscope)
{
	struct ifaddr *__single result = NULL;

	ifnet_head_lock_shared();

	result = ifa_ifwithaddr_scoped_locked(addr, ifscope);

	ifnet_head_done();

	return result;
}

struct ifaddr *
ifa_ifwithnet(const struct sockaddr *addr)
{
	return ifa_ifwithnet_common(addr, IFSCOPE_NONE);
}

struct ifaddr *
ifa_ifwithnet_scoped(const struct sockaddr *addr, unsigned int ifscope)
{
	return ifa_ifwithnet_common(addr, ifscope);
}

/*
 * Find an interface on a specific network.  If many, choice
 * is most specific found.
 */
static struct ifaddr *
ifa_ifwithnet_common(const struct sockaddr *addr, unsigned int ifscope)
{
	struct ifnet *__single ifp;
	struct ifaddr *__single ifa = NULL;
	struct ifaddr *__single ifa_maybe = NULL;
	u_int af = addr->sa_family;
	const char *cplim;
	const struct sockaddr_in6 *__single sock_addr = SIN6(addr);

	if (af != AF_INET && af != AF_INET6) {
		ifscope = IFSCOPE_NONE;
	}

	ifnet_head_lock_shared();
	/*
	 * AF_LINK addresses can be looked up directly by their index number,
	 * so do that if we can.
	 */
	if (af == AF_LINK) {
		const struct sockaddr_dl *sdl =
		    SDL(addr);
		if (sdl->sdl_index && sdl->sdl_index <= if_index) {
			ifa = ifnet_addrs[sdl->sdl_index - 1];
			if (ifa != NULL) {
				ifa_addref(ifa);
			}

			ifnet_head_done();
			return ifa;
		}
	}

	if (!in6_embedded_scope && af == AF_INET6 &&
	    IN6_IS_SCOPE_EMBED(&sock_addr->sin6_addr)) {
		VERIFY(ifscope != IFSCOPE_NONE);
	}

	/*
	 * Scan though each interface, looking for ones that have
	 * addresses in this address family.
	 */
	for (ifp = ifnet_head.tqh_first; ifp; ifp = ifp->if_link.tqe_next) {
		ifnet_lock_shared(ifp);
		for (ifa = ifp->if_addrhead.tqh_first; ifa;
		    ifa = ifa->ifa_link.tqe_next) {
			const char *cp, *cp2, *cp3;

			IFA_LOCK(ifa);
			if (ifa->ifa_addr == NULL ||
			    ifa->ifa_addr->sa_family != af) {
next:
				IFA_UNLOCK(ifa);
				continue;
			}
			/*
			 * If we're looking up with a scope,
			 * find using a matching interface.
			 */
			if (ifscope != IFSCOPE_NONE &&
			    ifp->if_index != ifscope) {
				IFA_UNLOCK(ifa);
				continue;
			}

			/*
			 * Scan all the bits in the ifa's address.
			 * If a bit dissagrees with what we are
			 * looking for, mask it with the netmask
			 * to see if it really matters.
			 * (A byte at a time)
			 */
			if (ifa->ifa_netmask == 0) {
				IFA_UNLOCK(ifa);
				continue;
			}
			cp = (const char*)SA_BYTES(addr)
			    + __offsetof(struct sockaddr, sa_data);
			cp2 = (const char*)SA_BYTES(ifa->ifa_addr)
			    + __offsetof(struct sockaddr, sa_data);
			cp3 = (const char*)SA_BYTES(ifa->ifa_netmask)
			    + __offsetof(struct sockaddr, sa_data);
			cplim = (const char*)SA_BYTES(ifa->ifa_netmask)
			    + ifa->ifa_netmask->sa_len;

			while (cp3 < cplim) {
				if ((*cp++ ^ *cp2++) & *cp3++) {
					goto next; /* next address! */
				}
			}
			/*
			 * If the netmask of what we just found
			 * is more specific than what we had before
			 * (if we had one) then remember the new one
			 * before continuing to search
			 * for an even better one.
			 */
			if (ifa_maybe == NULL ||
			    rn_refines((caddr_t)ifa->ifa_netmask,
			    (caddr_t)ifa_maybe->ifa_netmask)) {
				ifa_addref(ifa); /* ifa_maybe */
				IFA_UNLOCK(ifa);
				if (ifa_maybe != NULL) {
					ifa_remref(ifa_maybe);
				}
				ifa_maybe = ifa;
			} else {
				IFA_UNLOCK(ifa);
			}
			IFA_LOCK_ASSERT_NOTHELD(ifa);
		}
		ifnet_lock_done(ifp);

		if (ifa != NULL) {
			break;
		}
	}
	ifnet_head_done();

	if (ifa == NULL) {
		ifa = ifa_maybe;
	} else if (ifa_maybe != NULL) {
		ifa_remref(ifa_maybe);
	}

	return ifa;
}

/*
 * Find an interface address specific to an interface best matching
 * a given address applying same source address selection rules
 * as done in the kernel for implicit source address binding
 */
struct ifaddr *
ifaof_ifpforaddr_select(const struct sockaddr *addr, struct ifnet *ifp)
{
	u_int af = addr->sa_family;

	if (af == AF_INET6) {
		return in6_selectsrc_core_ifa(__DECONST(struct sockaddr_in6 *, addr), ifp, 0);
	}

	return ifaof_ifpforaddr(addr, ifp);
}

/*
 * Find an interface address specific to an interface best matching
 * a given address without regards to source address selection.
 *
 * This is appropriate for use-cases where we just want to update/init
 * some data structure like routing table entries.
 */
struct ifaddr *
ifaof_ifpforaddr(const struct sockaddr *addr, struct ifnet *ifp)
{
	struct ifaddr *__single ifa = NULL;
	const char *cp, *cp2, *cp3;
	const char *cplim;
	struct ifaddr *__single ifa_maybe = NULL;
	struct ifaddr *__single better_ifa_maybe = NULL;
	u_int af = addr->sa_family;

	if (af >= AF_MAX) {
		return NULL;
	}

	ifnet_lock_shared(ifp);
	for (ifa = ifp->if_addrhead.tqh_first; ifa;
	    ifa = ifa->ifa_link.tqe_next) {
		IFA_LOCK(ifa);
		if (ifa->ifa_addr->sa_family != af) {
			IFA_UNLOCK(ifa);
			continue;
		}
		if (ifa_maybe == NULL) {
			ifa_addref(ifa); /* for ifa_maybe */
			ifa_maybe = ifa;
		}
		if (ifa->ifa_netmask == 0) {
			if (sa_equal(addr, ifa->ifa_addr) ||
			    sa_equal(addr, ifa->ifa_dstaddr)) {
				ifa_addref(ifa); /* for caller */
				IFA_UNLOCK(ifa);
				break;
			}
			IFA_UNLOCK(ifa);
			continue;
		}
		if (ifp->if_flags & IFF_POINTOPOINT) {
			if (sa_equal(addr, ifa->ifa_dstaddr)) {
				ifa_addref(ifa); /* for caller */
				IFA_UNLOCK(ifa);
				break;
			}
		} else {
			if (sa_equal(addr, ifa->ifa_addr)) {
				/* exact match */
				ifa_addref(ifa); /* for caller */
				IFA_UNLOCK(ifa);
				break;
			}
			cp = (const char*)SA_BYTES(addr)
			    + __offsetof(struct sockaddr, sa_data);
			cp2 = (const char*)SA_BYTES(ifa->ifa_addr)
			    + __offsetof(struct sockaddr, sa_data);
			cp3 = (const char*)SA_BYTES(ifa->ifa_netmask)
			    + __offsetof(struct sockaddr, sa_data);
			cplim = (const char*)SA_BYTES(ifa->ifa_netmask)
			    + ifa->ifa_netmask->sa_len;

			for (; cp3 < cplim; cp3++) {
				if ((*cp++ ^ *cp2++) & *cp3) {
					break;
				}
			}
			if (cp3 == cplim) {
				/* subnet match */
				if (better_ifa_maybe == NULL) {
					/* for better_ifa_maybe */
					ifa_addref(ifa);
					better_ifa_maybe = ifa;
				}
			}
		}
		IFA_UNLOCK(ifa);
	}

	if (ifa == NULL) {
		if (better_ifa_maybe != NULL) {
			ifa = better_ifa_maybe;
			better_ifa_maybe = NULL;
		} else {
			ifa = ifa_maybe;
			ifa_maybe = NULL;
		}
	}

	ifnet_lock_done(ifp);

	if (better_ifa_maybe != NULL) {
		ifa_remref(better_ifa_maybe);
	}
	if (ifa_maybe != NULL) {
		ifa_remref(ifa_maybe);
	}

	return ifa;
}

#include <net/route.h>

/*
 * Default action when installing a route with a Link Level gateway.
 * Lookup an appropriate real ifa to point to.
 * This should be moved to /sys/net/link.c eventually.
 */
void
link_rtrequest(int cmd, struct rtentry *rt, struct sockaddr *sa)
{
	struct ifaddr *ifa;
	struct sockaddr *dst;
	struct ifnet *ifp;
	void (*ifa_rtrequest)(int, struct rtentry *, struct sockaddr *);

	LCK_MTX_ASSERT(rnh_lock, LCK_MTX_ASSERT_OWNED);
	RT_LOCK_ASSERT_HELD(rt);

	if (cmd != RTM_ADD || ((ifa = rt->rt_ifa) == 0) ||
	    ((ifp = ifa->ifa_ifp) == 0) || ((dst = rt_key(rt)) == 0)) {
		return;
	}

	/* Become a regular mutex, just in case */
	RT_CONVERT_LOCK(rt);

	ifa = ifaof_ifpforaddr(dst, ifp);
	if (ifa) {
		rtsetifa(rt, ifa);
		IFA_LOCK_SPIN(ifa);
		ifa_rtrequest = ifa->ifa_rtrequest;
		IFA_UNLOCK(ifa);
		if (ifa_rtrequest != NULL && ifa_rtrequest != link_rtrequest) {
			ifa_rtrequest(cmd, rt, sa);
		}
		ifa_remref(ifa);
	}
}

/*
 * if_updown will set the interface up or down. It will
 * prevent other up/down events from occurring until this
 * up/down event has completed.
 *
 * Caller must lock ifnet. This function will drop the
 * lock. This allows ifnet_set_flags to set the rest of
 * the flags after we change the up/down state without
 * dropping the interface lock between setting the
 * up/down state and updating the rest of the flags.
 */
__private_extern__ void
if_updown(struct ifnet *ifp, int up)
{
	u_int32_t eflags;
	int i;
	uint16_t addresses_count = 0;
	ifaddr_t *__counted_by(addresses_count) ifa = NULL;

	struct timespec tv;
	struct ifclassq *ifq;

	/* Wait until no one else is changing the up/down state */
	while ((ifp->if_eflags & IFEF_UPDOWNCHANGE) != 0) {
		tv.tv_sec = 0;
		tv.tv_nsec = NSEC_PER_SEC / 10;
		ifnet_lock_done(ifp);
		msleep(&ifp->if_eflags, NULL, 0, "if_updown", &tv);
		ifnet_lock_exclusive(ifp);
	}

	/* Verify that the interface isn't already in the right state */
	if ((!up && (ifp->if_flags & IFF_UP) == 0) ||
	    (up && (ifp->if_flags & IFF_UP) == IFF_UP)) {
		return;
	}

	/* Indicate that the up/down state is changing */
	eflags = if_set_eflags(ifp, IFEF_UPDOWNCHANGE);
	ASSERT((eflags & IFEF_UPDOWNCHANGE) == 0);

	/* Mark interface up or down */
	if (up) {
		ifp->if_flags |= IFF_UP;
	} else {
		ifp->if_flags &= ~IFF_UP;
	}

	if (!ifnet_is_attached(ifp, 1)) {
		/*
		 * The interface is not attached or is detaching, so
		 * skip modifying any other state.
		 */
		os_log(OS_LOG_DEFAULT, "%s: %s is not attached",
		    __func__, if_name(ifp));
	} else {
		/* Drop the lock to notify addresses and route */
		ifnet_lock_done(ifp);

		/* Inform all transmit queues about the new link state */
		ifq = ifp->if_snd;
		ASSERT(ifq != NULL);
		IFCQ_LOCK(ifq);
		if_qflush_snd(ifp, true);
		ifnet_update_sndq(ifq,
		    up ? CLASSQ_EV_LINK_UP : CLASSQ_EV_LINK_DOWN);
		IFCQ_UNLOCK(ifq);

		/* Inform protocols of changed interface state */
		if (ifnet_get_address_list_family_with_count(ifp, &ifa, &addresses_count, 0) == 0) {
			for (i = 0; ifa[i] != 0; i++) {
				pfctlinput(up ? PRC_IFUP : PRC_IFDOWN,
				    ifa[i]->ifa_addr);
			}
			ifnet_address_list_free_counted_by(ifa, addresses_count);
		}
		rt_ifmsg(ifp);

		ifnet_lock_exclusive(ifp);
		ifnet_touch_lastchange(ifp);
		ifnet_touch_lastupdown(ifp);
		ifnet_decr_iorefcnt(ifp);
	}
	if_clear_eflags(ifp, IFEF_UPDOWNCHANGE);
	wakeup(&ifp->if_eflags);
}

/*
 * Mark an interface down and notify protocols of
 * the transition.
 */
void
if_down(
	struct ifnet *ifp)
{
	ifnet_lock_exclusive(ifp);
	if_updown(ifp, 0);
	ifnet_lock_done(ifp);
}

/*
 * Mark an interface up and notify protocols of
 * the transition.
 */
void
if_up(
	struct ifnet *ifp)
{
	ifnet_lock_exclusive(ifp);
	if_updown(ifp, 1);
	ifnet_lock_done(ifp);
}

/*
 * Flush an interface queue.
 */
void
if_qflush(struct ifnet *ifp, struct ifclassq *ifq, bool ifq_locked)
{
	lck_mtx_lock(&ifp->if_ref_lock);
	if ((ifp->if_refflags & IFRF_ATTACH_MASK) == 0) {
		lck_mtx_unlock(&ifp->if_ref_lock);
		return;
	}
	VERIFY(ifq != NULL);
	ifclassq_retain(ifq);
	lck_mtx_unlock(&ifp->if_ref_lock);

	if (!ifq_locked) {
		IFCQ_LOCK(ifq);
	}

	if (IFCQ_IS_ENABLED(ifq)) {
		fq_if_request_classq(ifq, CLASSQRQ_PURGE, NULL);
	}

	VERIFY(IFCQ_IS_EMPTY(ifq));

	if (!ifq_locked) {
		IFCQ_UNLOCK(ifq);
	}
	ifclassq_release(&ifq);
}

void
if_qflush_snd(struct ifnet *ifp, bool ifq_locked)
{
	if_qflush(ifp, ifp->if_snd, ifq_locked);
}

void
if_qflush_sc(struct ifnet *ifp, mbuf_svc_class_t sc, u_int32_t flow,
    u_int32_t *packets, u_int32_t *bytes, int ifq_locked)
{
	struct ifclassq *ifq;
	u_int32_t cnt = 0, len = 0;

	if ((ifp->if_refflags & IFRF_ATTACH_MASK) == 0) {
		return;
	}
	ifq = ifp->if_snd;
	VERIFY(ifq != NULL);
	VERIFY(sc == MBUF_SC_UNSPEC || MBUF_VALID_SC(sc));
	VERIFY(flow != 0);

	if (!ifq_locked) {
		IFCQ_LOCK(ifq);
	}

	if (IFCQ_IS_ENABLED(ifq)) {
		cqrq_purge_sc_t req = { sc, flow, 0, 0 };

		fq_if_request_classq(ifq, CLASSQRQ_PURGE_SC, &req);
		cnt = req.packets;
		len = req.bytes;
	}

	if (!ifq_locked) {
		IFCQ_UNLOCK(ifq);
	}

	if (packets != NULL) {
		*packets = cnt;
	}
	if (bytes != NULL) {
		*bytes = len;
	}
}

/*
 * Extracts interface unit number and name from string, returns -1 upon failure.
 * Upon success, returns extracted unit number, and interface name in dst.
 */
int
ifunit_extract(const char *src0, char *__counted_by(dstlen)dst, size_t dstlen, int *unit)
{
	const char* cp;
	size_t len, m;
	char c;
	int u;

	if (src0 == NULL || dst == NULL || dstlen == 0 || unit == NULL) {
		return -1;
	}
	len = strlen(src0);
	if (len < 2 || len > dstlen) {
		return -1;
	}

	const char *src = __unsafe_null_terminated_to_indexable(src0);
	c = *src;
	if (c >= '0' && c <= '9') {
		return -1; /* starts with number */
	}

	cp = src + len - 1;
	c = *cp;
	if (c < '0' || c > '9') {
		return -1;            /* trailing garbage */
	}
	u = 0;
	m = 1;
	do {
		if (cp == src) {
			return -1;    /* no interface name */
		}
		u += (c - '0') * m;
		if (u > 1000000) {
			return -1;    /* number is unreasonable */
		}
		m *= 10;
		c = *--cp;
	} while (c >= '0' && c <= '9');
	len = cp - src + 1;
	strbufcpy(dst, dstlen, src, len);
	dst[len] = '\0';
	*unit = u;

	return 0;
}

/*
 * Map interface name to
 * interface structure pointer.
 */
static struct ifnet *
ifunit_common(const char *name, boolean_t hold)
{
	char namebuf[IFNAMSIZ + 1];
	struct ifnet *ifp;
	int unit;

	if (ifunit_extract(name, namebuf, sizeof(namebuf), &unit) < 0) {
		return NULL;
	}

	/* for safety */
	namebuf[sizeof(namebuf) - 1] = '\0';

	/*
	 * Now search all the interfaces for this name/number
	 */
	ifnet_head_lock_shared();
	TAILQ_FOREACH(ifp, &ifnet_head, if_link) {
		/*
		 * Use strlcmp() with sizeof(namebuf) since we
		 * want to match the entire string.
		 */
		if (strlcmp(namebuf, ifp->if_name, sizeof(namebuf))) {
			continue;
		}
		if (unit == ifp->if_unit) {
			break;
		}
	}

	/* if called from ifunit_ref() and ifnet is not attached, bail */
	if (hold && ifp != NULL && !ifnet_is_attached(ifp, 1)) {
		ifp = NULL;
	}

	ifnet_head_done();
	return ifp;
}

struct ifnet *
ifunit(const char *name)
{
	return ifunit_common(name, FALSE);
}

/*
 * Similar to ifunit(), except that we hold an I/O reference count on an
 * attached interface, which must later be released via ifnet_decr_iorefcnt().
 * Will return NULL unless interface exists and is fully attached.
 */
struct ifnet *
ifunit_ref(const char *name)
{
	return ifunit_common(name, TRUE);
}

/*
 * Map interface name in a sockaddr_dl to
 * interface structure pointer.
 */
struct ifnet *
if_withname(struct sockaddr *sa)
{
	char ifname[IFNAMSIZ + 1];
	struct sockaddr_dl *sdl = SDL(sa);

	if ((sa->sa_family != AF_LINK) || (sdl->sdl_nlen == 0) ||
	    (sdl->sdl_nlen > IFNAMSIZ)) {
		return NULL;
	}

	/*
	 * ifunit wants a null-terminated name.  It may not be null-terminated
	 * in the sockaddr.  We don't want to change the caller's sockaddr,
	 * and there might not be room to put the trailing null anyway, so we
	 * make a local copy that we know we can null terminate safely.
	 */
	return ifunit(strbufcpy(ifname, sizeof(ifname), sdl->sdl_data, sdl->sdl_nlen));
}

static __attribute__((noinline)) int
ifioctl_ifconf(u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data)
{
	int error = 0;

	switch (cmd) {
	case OSIOCGIFCONF32:                    /* struct ifconf32 */
	case SIOCGIFCONF32: {                   /* struct ifconf32 */
		struct ifconf32 ifc;
		bcopy(data, &ifc, sizeof(ifc));
		error = ifconf(cmd, CAST_USER_ADDR_T(ifc.ifc_req),
		    &ifc.ifc_len);
		bcopy(&ifc, data, sizeof(ifc));
		break;
	}

	case SIOCGIFCONF64:                     /* struct ifconf64 */
	case OSIOCGIFCONF64: {                  /* struct ifconf64 */
		struct ifconf64 ifc;
		bcopy(data, &ifc, sizeof(ifc));
		error = ifconf(cmd, CAST_USER_ADDR_T(ifc.ifc_req), &ifc.ifc_len);
		bcopy(&ifc, data, sizeof(ifc));
		break;
	}

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

static __attribute__((noinline)) int
ifioctl_ifclone(u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data)
{
	int error = 0;

	switch (cmd) {
	case SIOCIFGCLONERS32: {                /* struct if_clonereq32 */
		struct if_clonereq32 ifcr;
		bcopy(data, &ifcr, sizeof(ifcr));
		error = if_clone_list(ifcr.ifcr_count, &ifcr.ifcr_total,
		    CAST_USER_ADDR_T(ifcr.ifcru_buffer));
		bcopy(&ifcr, data, sizeof(ifcr));
		break;
	}

	case SIOCIFGCLONERS64: {                /* struct if_clonereq64 */
		struct if_clonereq64 ifcr;
		bcopy(data, &ifcr, sizeof(ifcr));
		error = if_clone_list(ifcr.ifcr_count, &ifcr.ifcr_total,
		    CAST_USER_ADDR_T(ifcr.ifcru_buffer));
		bcopy(&ifcr, data, sizeof(ifcr));
		break;
	}

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

static __attribute__((noinline)) int
ifioctl_ifdesc(struct ifnet *ifp, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data, struct proc *p)
{
	struct if_descreq *ifdr = (struct if_descreq *)(void *)data;
	u_int32_t ifdr_len;
	int error = 0;

	VERIFY(ifp != NULL);

	switch (cmd) {
	case SIOCSIFDESC: {                     /* struct if_descreq */
		if ((error = proc_suser(p)) != 0) {
			break;
		}

		ifnet_lock_exclusive(ifp);
		bcopy(&ifdr->ifdr_len, &ifdr_len, sizeof(ifdr_len));
		if (ifdr_len > sizeof(ifdr->ifdr_desc) ||
		    ifdr_len > ifp->if_desc.ifd_maxlen) {
			error = EINVAL;
			ifnet_lock_done(ifp);
			break;
		}

		bzero(ifp->if_desc.ifd_desc, ifp->if_desc.ifd_maxlen);
		if ((ifp->if_desc.ifd_len = ifdr_len) > 0) {
			bcopy(ifdr->ifdr_desc, ifp->if_desc.ifd_desc,
			    MIN(ifdr_len, ifp->if_desc.ifd_maxlen));
		}
		ifnet_lock_done(ifp);
		break;
	}

	case SIOCGIFDESC: {                     /* struct if_descreq */
		ifnet_lock_shared(ifp);
		ifdr_len = MIN(ifp->if_desc.ifd_len, sizeof(ifdr->ifdr_desc));
		bcopy(&ifdr_len, &ifdr->ifdr_len, sizeof(ifdr_len));
		bzero(&ifdr->ifdr_desc, sizeof(ifdr->ifdr_desc));
		if (ifdr_len > 0) {
			bcopy(ifp->if_desc.ifd_desc, ifdr->ifdr_desc, ifdr_len);
		}
		ifnet_lock_done(ifp);
		break;
	}

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

static __attribute__((noinline)) int
ifioctl_linkparams(struct ifnet *ifp, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data, struct proc *p)
{
	struct if_linkparamsreq *iflpr =
	    (struct if_linkparamsreq *)(void *)data;
	struct ifclassq *ifq;
	int error = 0;

	VERIFY(ifp != NULL);
	ifq = ifp->if_snd;

	ASSERT(ifq != NULL);
	switch (cmd) {
	case SIOCSIFLINKPARAMS: {               /* struct if_linkparamsreq */
		struct tb_profile tb = { .rate = 0, .percent = 0, .depth = 0 };

		if ((error = proc_suser(p)) != 0) {
			break;
		}

#if SKYWALK
		error = kern_nexus_set_netif_input_tbr_rate(ifp,
		    iflpr->iflpr_input_tbr_rate);
		if (error != 0) {
			break;
		}

		/*
		 * Input netem is done at flowswitch, which is the entry point
		 * of all traffic, when skywalk is enabled.
		 */
		error = kern_nexus_set_if_netem_params(
			kern_nexus_shared_controller(),
			ifp->if_nx_flowswitch.if_fsw_instance,
			&iflpr->iflpr_input_netem,
			sizeof(iflpr->iflpr_input_netem));
		if (error != 0) {
			break;
		}
#endif /* SKYWALK */

		char netem_name[32];
		const char *__null_terminated ifname = NULL;
		ifname = tsnprintf(netem_name, sizeof(netem_name),
		    "if_output_netem_%s", if_name(ifp));
		error = netem_config(&ifp->if_output_netem, ifname, ifp,
		    &iflpr->iflpr_output_netem, (void *)ifp,
		    ifnet_enqueue_netem, NETEM_MAX_BATCH_SIZE);
		if (error != 0) {
			break;
		}

		IFCQ_LOCK(ifq);
		if (!IFCQ_IS_READY(ifq)) {
			error = ENXIO;
			IFCQ_UNLOCK(ifq);
			break;
		}
		bcopy(&iflpr->iflpr_output_tbr_rate, &tb.rate,
		    sizeof(tb.rate));
		bcopy(&iflpr->iflpr_output_tbr_percent, &tb.percent,
		    sizeof(tb.percent));
		error = ifclassq_tbr_set(ifq, &tb, TRUE);
		IFCQ_UNLOCK(ifq);
		break;
	}

	case SIOCGIFLINKPARAMS: {               /* struct if_linkparamsreq */
		u_int32_t sched_type = PKTSCHEDT_NONE, flags = 0;
		u_int64_t tbr_bw = 0, tbr_pct = 0;

		IFCQ_LOCK(ifq);

		if (IFCQ_IS_ENABLED(ifq)) {
			sched_type = ifq->ifcq_type;
		}

		bcopy(&sched_type, &iflpr->iflpr_output_sched,
		    sizeof(iflpr->iflpr_output_sched));

		if (IFCQ_TBR_IS_ENABLED(ifq)) {
			tbr_bw = ifq->ifcq_tbr.tbr_rate_raw;
			tbr_pct = ifq->ifcq_tbr.tbr_percent;
		}
		bcopy(&tbr_bw, &iflpr->iflpr_output_tbr_rate,
		    sizeof(iflpr->iflpr_output_tbr_rate));
		bcopy(&tbr_pct, &iflpr->iflpr_output_tbr_percent,
		    sizeof(iflpr->iflpr_output_tbr_percent));
		IFCQ_UNLOCK(ifq);

		if (ifp->if_output_sched_model ==
		    IFNET_SCHED_MODEL_DRIVER_MANAGED) {
			flags |= IFLPRF_DRVMANAGED;
		}
		bcopy(&flags, &iflpr->iflpr_flags, sizeof(iflpr->iflpr_flags));
		bcopy(&ifp->if_output_bw, &iflpr->iflpr_output_bw,
		    sizeof(iflpr->iflpr_output_bw));
		bcopy(&ifp->if_input_bw, &iflpr->iflpr_input_bw,
		    sizeof(iflpr->iflpr_input_bw));
		bcopy(&ifp->if_output_lt, &iflpr->iflpr_output_lt,
		    sizeof(iflpr->iflpr_output_lt));
		bcopy(&ifp->if_input_lt, &iflpr->iflpr_input_lt,
		    sizeof(iflpr->iflpr_input_lt));

#if SKYWALK
		if (ifp->if_input_netem != NULL) {
			netem_get_params(ifp->if_input_netem,
			    &iflpr->iflpr_input_netem);
		}
#endif /* SKYWALK */
		if (ifp->if_output_netem != NULL) {
			netem_get_params(ifp->if_output_netem,
			    &iflpr->iflpr_output_netem);
		}

		break;
	}

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

static __attribute__((noinline)) int
ifioctl_qstats(struct ifnet *ifp, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data)
{
	struct if_qstatsreq *ifqr = (struct if_qstatsreq *)(void *)data;
	u_int32_t ifqr_len, ifqr_slot;
	uint32_t ifqr_grp_idx = 0;
	int error = 0;

	VERIFY(ifp != NULL);

	switch (cmd) {
	case SIOCGIFQUEUESTATS: {               /* struct if_qstatsreq */
		bcopy(&ifqr->ifqr_slot, &ifqr_slot, sizeof(ifqr_slot));
		bcopy(&ifqr->ifqr_grp_idx, &ifqr_grp_idx, sizeof(ifqr_grp_idx));
		bcopy(&ifqr->ifqr_len, &ifqr_len, sizeof(ifqr_len));

		if (ifqr_grp_idx > FQ_IF_MAX_GROUPS) {
			return EINVAL;
		}
		error = ifclassq_getqstats(ifp->if_snd, (uint8_t)ifqr_grp_idx,
		    ifqr_slot, ifqr->ifqr_buf, &ifqr_len);
		if (error != 0) {
			ifqr_len = 0;
		}
		bcopy(&ifqr_len, &ifqr->ifqr_len, sizeof(ifqr_len));
		break;
	}

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

static __attribute__((noinline)) int
ifioctl_throttle(struct ifnet *ifp, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data, struct proc *p)
{
	struct if_throttlereq *ifthr = (struct if_throttlereq *)(void *)data;
	u_int32_t ifthr_level;
	int error = 0;

	VERIFY(ifp != NULL);

	switch (cmd) {
	case SIOCSIFTHROTTLE: {                 /* struct if_throttlereq */
		/*
		 * XXX: Use priv_check_cred() instead of root check?
		 */
		if ((error = proc_suser(p)) != 0) {
			break;
		}

		bcopy(&ifthr->ifthr_level, &ifthr_level, sizeof(ifthr_level));
		error = ifnet_set_throttle(ifp, ifthr_level);
		if (error == EALREADY) {
			error = 0;
		}
		break;
	}

	case SIOCGIFTHROTTLE: {                 /* struct if_throttlereq */
		if ((error = ifnet_get_throttle(ifp, &ifthr_level)) == 0) {
			bcopy(&ifthr_level, &ifthr->ifthr_level,
			    sizeof(ifthr_level));
		}
		break;
	}

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

static int
ifioctl_getnetagents(struct ifnet *ifp, u_int32_t *count, user_addr_t uuid_p)
{
	int error = 0;
	u_int32_t index = 0;
	u_int32_t valid_netagent_count = 0;
	*count = 0;

	ifnet_lock_assert(ifp, IFNET_LCK_ASSERT_SHARED);

	if (ifp->if_agentids != NULL) {
		for (index = 0; index < ifp->if_agentcount; index++) {
			uuid_t *netagent_uuid = &(ifp->if_agentids[index]);
			if (!uuid_is_null(*netagent_uuid)) {
				if (uuid_p != USER_ADDR_NULL) {
					error = copyout(netagent_uuid,
					    uuid_p + sizeof(uuid_t) * valid_netagent_count,
					    sizeof(uuid_t));
					if (error != 0) {
						return error;
					}
				}
				valid_netagent_count++;
			}
		}
	}
	*count = valid_netagent_count;

	return 0;
}

#define IF_MAXAGENTS            64
#define IF_AGENT_INCREMENT      8
int
if_add_netagent_locked(struct ifnet *ifp, uuid_t new_agent_uuid)
{
	VERIFY(ifp != NULL);

	uuid_t *__single first_empty_slot = NULL;
	u_int32_t index = 0;
	bool already_added = FALSE;

	if (ifp->if_agentids != NULL) {
		for (index = 0; index < ifp->if_agentcount; index++) {
			uuid_t *netagent_uuid = &(ifp->if_agentids[index]);
			if (uuid_compare(*netagent_uuid, new_agent_uuid) == 0) {
				/* Already present, ignore */
				already_added = TRUE;
				break;
			}
			if (first_empty_slot == NULL &&
			    uuid_is_null(*netagent_uuid)) {
				first_empty_slot = netagent_uuid;
			}
		}
	}
	if (already_added) {
		/* Already added agent, don't return an error */
		return 0;
	}
	if (first_empty_slot == NULL) {
		if (ifp->if_agentcount >= IF_MAXAGENTS) {
			/* No room for another netagent UUID, bail */
			return ENOMEM;
		} else {
			/* Calculate new array size */
			u_int32_t current_slot;
			u_int32_t new_agent_count =
			    MIN(ifp->if_agentcount + IF_AGENT_INCREMENT,
			    IF_MAXAGENTS);

			/* Reallocate array */
			uuid_t *new_agent_array = krealloc_data(ifp->if_agentids,
			    sizeof(uuid_t) * ifp->if_agentcount,
			    sizeof(uuid_t) * new_agent_count,
			    Z_WAITOK | Z_ZERO);
			if (new_agent_array == NULL) {
				return ENOMEM;
			}

			current_slot = ifp->if_agentcount;

			/* Save new array */
			ifp->if_agentids = new_agent_array;
			ifp->if_agentcount = new_agent_count;

			/* Set first empty slot */
			first_empty_slot = &(ifp->if_agentids[current_slot]);
		}
	}
	uuid_copy(*first_empty_slot, new_agent_uuid);
	netagent_post_updated_interfaces(new_agent_uuid);
	return 0;
}

int
if_add_netagent(struct ifnet *ifp, uuid_t new_agent_uuid)
{
	VERIFY(ifp != NULL);

	ifnet_lock_exclusive(ifp);

	int error = if_add_netagent_locked(ifp, new_agent_uuid);

	ifnet_lock_done(ifp);

	return error;
}

static int
if_delete_netagent_locked(struct ifnet *ifp, uuid_t remove_agent_uuid)
{
	u_int32_t index = 0;
	bool removed_agent_id = FALSE;

	if (ifp->if_agentids != NULL) {
		for (index = 0; index < ifp->if_agentcount; index++) {
			uuid_t *netagent_uuid = &(ifp->if_agentids[index]);
			if (uuid_compare(*netagent_uuid,
			    remove_agent_uuid) == 0) {
				uuid_clear(*netagent_uuid);
				removed_agent_id = TRUE;
				break;
			}
		}
	}
	if (removed_agent_id) {
		netagent_post_updated_interfaces(remove_agent_uuid);
	}

	return 0;
}

int
if_delete_netagent(struct ifnet *ifp, uuid_t remove_agent_uuid)
{
	VERIFY(ifp != NULL);

	ifnet_lock_exclusive(ifp);

	int error = if_delete_netagent_locked(ifp, remove_agent_uuid);

	ifnet_lock_done(ifp);

	return error;
}

boolean_t
if_check_netagent(struct ifnet *ifp, uuid_t find_agent_uuid)
{
	boolean_t found = FALSE;

	if (!ifp || uuid_is_null(find_agent_uuid)) {
		return FALSE;
	}

	ifnet_lock_shared(ifp);

	if (ifp->if_agentids != NULL) {
		for (uint32_t index = 0; index < ifp->if_agentcount; index++) {
			if (uuid_compare(ifp->if_agentids[index], find_agent_uuid) == 0) {
				found = TRUE;
				break;
			}
		}
	}

	ifnet_lock_done(ifp);

	return found;
}

static __attribute__((noinline)) int
ifioctl_netagent(struct ifnet *ifp, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data, struct proc *p)
{
	struct if_agentidreq *ifar = (struct if_agentidreq *)(void *)data;
	union {
		struct if_agentidsreq32 s32;
		struct if_agentidsreq64 s64;
	} u;
	int error = 0;

	VERIFY(ifp != NULL);

	/* Get an io ref count if the interface is attached */
	if (!ifnet_is_attached(ifp, 1)) {
		return EOPNOTSUPP;
	}

	if (cmd == SIOCAIFAGENTID ||
	    cmd == SIOCDIFAGENTID) {
		ifnet_lock_exclusive(ifp);
	} else {
		ifnet_lock_shared(ifp);
	}

	switch (cmd) {
	case SIOCAIFAGENTID: {                  /* struct if_agentidreq */
		// TODO: Use priv_check_cred() instead of root check
		if ((error = proc_suser(p)) != 0) {
			break;
		}
		error = if_add_netagent_locked(ifp, ifar->ifar_uuid);
		break;
	}
	case SIOCDIFAGENTID: {                          /* struct if_agentidreq */
		// TODO: Use priv_check_cred() instead of root check
		if ((error = proc_suser(p)) != 0) {
			break;
		}
		error = if_delete_netagent_locked(ifp, ifar->ifar_uuid);
		break;
	}
	case SIOCGIFAGENTIDS32: {               /* struct if_agentidsreq32 */
		bcopy(data, &u.s32, sizeof(u.s32));
		error = ifioctl_getnetagents(ifp, &u.s32.ifar_count,
		    u.s32.ifar_uuids);
		if (error == 0) {
			bcopy(&u.s32, data, sizeof(u.s32));
		}
		break;
	}
	case SIOCGIFAGENTIDS64: {               /* struct if_agentidsreq64 */
		bcopy(data, &u.s64, sizeof(u.s64));
		error = ifioctl_getnetagents(ifp, &u.s64.ifar_count,
		    CAST_USER_ADDR_T(u.s64.ifar_uuids));
		if (error == 0) {
			bcopy(&u.s64, data, sizeof(u.s64));
		}
		break;
	}
	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	ifnet_lock_done(ifp);
	ifnet_decr_iorefcnt(ifp);

	return error;
}

void
ifnet_clear_netagent(uuid_t netagent_uuid)
{
	struct ifnet *ifp = NULL;
	u_int32_t index = 0;

	ifnet_head_lock_shared();

	TAILQ_FOREACH(ifp, &ifnet_head, if_link) {
		ifnet_lock_shared(ifp);
		if (ifp->if_agentids != NULL) {
			for (index = 0; index < ifp->if_agentcount; index++) {
				uuid_t *ifp_netagent_uuid = &(ifp->if_agentids[index]);
				if (uuid_compare(*ifp_netagent_uuid, netagent_uuid) == 0) {
					uuid_clear(*ifp_netagent_uuid);
				}
			}
		}
		ifnet_lock_done(ifp);
	}

	ifnet_head_done();
}

void
ifnet_increment_generation(ifnet_t interface)
{
	OSIncrementAtomic(&interface->if_generation);
}

u_int32_t
ifnet_get_generation(ifnet_t interface)
{
	return interface->if_generation;
}

void
ifnet_remove_from_ordered_list(struct ifnet *ifp)
{
	ifnet_head_assert_exclusive();

	// Remove from list
	TAILQ_REMOVE(&ifnet_ordered_head, ifp, if_ordered_link);
	ifp->if_ordered_link.tqe_next = NULL;
	ifp->if_ordered_link.tqe_prev = NULL;

	// Update ordered count
	VERIFY(if_ordered_count > 0);
	if_ordered_count--;
}

static int
ifnet_reset_order(u_int32_t *__counted_by(count) ordered_indices, u_int32_t count)
{
	struct ifnet *ifp = NULL;
	int error = 0;

	if (if_verbose != 0) {
		os_log(OS_LOG_DEFAULT, "%s: count %u", __func__, count);
	}

	ifnet_head_lock_exclusive();
	for (u_int32_t order_index = 0; order_index < count; order_index++) {
		if (ordered_indices[order_index] == IFSCOPE_NONE ||
		    ordered_indices[order_index] > (uint32_t)if_index) {
			error = EINVAL;
			ifnet_head_done();
			return error;
		}
	}
	// Flush current ordered list
	for (ifp = TAILQ_FIRST(&ifnet_ordered_head); ifp != NULL;
	    ifp = TAILQ_FIRST(&ifnet_ordered_head)) {
		ifnet_lock_exclusive(ifp);
		ifnet_remove_from_ordered_list(ifp);
		ifnet_lock_done(ifp);
	}

	VERIFY(if_ordered_count == 0);

	for (u_int32_t order_index = 0; order_index < count; order_index++) {
		u_int32_t interface_index = ordered_indices[order_index];
		ifp = ifindex2ifnet[interface_index];
		if (ifp == NULL) {
			continue;
		}
		ifnet_lock_exclusive(ifp);
		TAILQ_INSERT_TAIL(&ifnet_ordered_head, ifp, if_ordered_link);
		ifnet_lock_done(ifp);
		if_ordered_count++;
	}

	ifnet_head_done();

	necp_update_all_clients();

	return error;
}

#if (DEBUG || DEVELOPMENT)
static int
ifnet_get_ordered_indices(u_int32_t *__indexable ordered_indices, uint32_t *count)
{
	struct ifnet *ifp = NULL;
	int error = 0;
	uint32_t order_index = 0;

	ifnet_head_lock_exclusive();

	if (*count < if_ordered_count) {
		ifnet_head_done();
		return ENOBUFS;
	}

	TAILQ_FOREACH(ifp, &ifnet_ordered_head, if_ordered_link) {
		if (order_index >= if_ordered_count) {
			break;
		}
		ordered_indices[order_index++] = ifp->if_index;
	}
	*count = order_index;
	ifnet_head_done();

	return error;
}
#endif /* (DEBUG || DEVELOPMENT) */

int
if_set_qosmarking_mode(struct ifnet *ifp, u_int32_t mode)
{
	int error = 0;
	u_int32_t old_mode = ifp->if_qosmarking_mode;

	switch (mode) {
	case IFRTYPE_QOSMARKING_MODE_NONE:
		ifp->if_qosmarking_mode = IFRTYPE_QOSMARKING_MODE_NONE;
		break;
	case IFRTYPE_QOSMARKING_FASTLANE:
	case IFRTYPE_QOSMARKING_RFC4594:
		ifp->if_qosmarking_mode = mode;
		break;
#if (DEBUG || DEVELOPMENT)
	case IFRTYPE_QOSMARKING_CUSTOM:
		ifp->if_qosmarking_mode = mode;
		break;
#endif /* (DEBUG || DEVELOPMENT) */
	default:
		error = EINVAL;
		break;
	}
	if (error == 0 && old_mode != ifp->if_qosmarking_mode) {
		dlil_post_msg(ifp, KEV_DL_SUBCLASS, KEV_DL_QOS_MODE_CHANGED,
		    NULL, 0, FALSE);
	}
	return error;
}

static __attribute__((noinline)) int
ifioctl_iforder(u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data)
{
	int error = 0;
	u_int32_t *ordered_indices = NULL;
	size_t ordered_indices_length = 0;

	if (data == NULL) {
		return EINVAL;
	}

	switch (cmd) {
	case SIOCSIFORDER: {            /* struct if_order */
		struct if_order *ifo = (struct if_order *)(void *)data;

		if (ifo->ifo_count > (u_int32_t)if_index) {
			error = EINVAL;
			break;
		}

		ordered_indices_length = ifo->ifo_count * sizeof(u_int32_t);
		if (ordered_indices_length > 0) {
			if (ifo->ifo_ordered_indices == USER_ADDR_NULL) {
				error = EINVAL;
				break;
			}
			ordered_indices = (u_int32_t *)kalloc_data(ordered_indices_length,
			    Z_WAITOK);
			if (ordered_indices == NULL) {
				error = ENOMEM;
				break;
			}

			error = copyin(CAST_USER_ADDR_T(ifo->ifo_ordered_indices),
			    ordered_indices, ordered_indices_length);
			if (error != 0) {
				break;
			}

			/* ordered_indices should not contain duplicates */
			bool found_duplicate = FALSE;
			for (uint32_t i = 0; i < (ifo->ifo_count - 1) && !found_duplicate; i++) {
				for (uint32_t j = i + 1; j < ifo->ifo_count && !found_duplicate; j++) {
					if (ordered_indices[j] == ordered_indices[i]) {
						error = EINVAL;
						found_duplicate = TRUE;
						break;
					}
				}
			}
			if (found_duplicate) {
				break;
			}

			error = ifnet_reset_order(ordered_indices, ifo->ifo_count);
		} else {
			// Clear the list
			error = ifnet_reset_order(NULL, 0);
		}
		break;
	}

	case SIOCGIFORDER: {
#if (DEBUG || DEVELOPMENT)
		struct if_order *ifo = (struct if_order *)(void *)data;
		uint32_t count;

		if (ifo->ifo_ordered_indices == 0) {
			ifo->ifo_count = if_ordered_count;
			break;
		}

		count = ifo->ifo_count;
		if (count == 0) {
			error = EINVAL;
			break;
		}

		ordered_indices_length = count * sizeof(uint32_t);
		ordered_indices = (uint32_t *)kalloc_data(ordered_indices_length,
		    Z_WAITOK | Z_ZERO);
		if (ordered_indices == NULL) {
			error = ENOMEM;
			break;
		}

		error = ifnet_get_ordered_indices(ordered_indices, &count);
		if (error == 0) {
			ifo->ifo_count = count;
			error = copyout((caddr_t)ordered_indices,
			    CAST_USER_ADDR_T(ifo->ifo_ordered_indices),
			    count * sizeof(uint32_t));
		}
#else /* (DEBUG || DEVELOPMENT) */
		error = EOPNOTSUPP;
#endif /* (DEBUG || DEVELOPMENT) */

		break;
	}

	default: {
		VERIFY(0);
		/* NOTREACHED */
	}
	}

	if (ordered_indices != NULL) {
		kfree_data(ordered_indices, ordered_indices_length);
	}

	return error;
}

static __attribute__((noinline)) int
ifioctl_networkid(struct ifnet *ifp, caddr_t __indexable data)
{
	struct if_netidreq *ifnetidr = (struct if_netidreq *)(void *)data;
	int error = 0;
	int len = ifnetidr->ifnetid_len;

	VERIFY(ifp != NULL);

	if (len > sizeof(ifnetidr->ifnetid)) {
		error = EINVAL;
		goto end;
	}

	if (len == 0) {
		bzero(&ifp->network_id, sizeof(ifp->network_id));
	} else if (len > sizeof(ifp->network_id)) {
		error = EINVAL;
		goto end;
	}

	ifp->network_id_len = (uint8_t)len;
	bcopy(data, ifp->network_id, len);
end:
	return error;
}

static __attribute__((noinline)) int
ifioctl_netsignature(struct ifnet *ifp, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data)
{
	struct if_nsreq *ifnsr = (struct if_nsreq *)(void *)data;
	u_int16_t flags;
	int error = 0;

	VERIFY(ifp != NULL);

	switch (cmd) {
	case SIOCSIFNETSIGNATURE:               /* struct if_nsreq */
		if (ifnsr->ifnsr_len > sizeof(ifnsr->ifnsr_data)) {
			error = EINVAL;
			break;
		}
		bcopy(&ifnsr->ifnsr_flags, &flags, sizeof(flags));
		error = ifnet_set_netsignature(ifp, ifnsr->ifnsr_family,
		    ifnsr->ifnsr_len, flags, ifnsr->ifnsr_data);
		break;

	case SIOCGIFNETSIGNATURE:               /* struct if_nsreq */
		ifnsr->ifnsr_len = sizeof(ifnsr->ifnsr_data);
		error = ifnet_get_netsignature(ifp, ifnsr->ifnsr_family,
		    &ifnsr->ifnsr_len, &flags, ifnsr->ifnsr_data);
		if (error == 0) {
			bcopy(&flags, &ifnsr->ifnsr_flags, sizeof(flags));
		} else {
			ifnsr->ifnsr_len = 0;
		}
		break;

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

static __attribute__((noinline)) int
ifioctl_nat64prefix(struct ifnet *ifp, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data)
{
	struct if_nat64req *ifnat64 = (struct if_nat64req *)(void *)data;
	int error = 0;

	VERIFY(ifp != NULL);

	switch (cmd) {
	case SIOCSIFNAT64PREFIX:                /* struct if_nat64req */
		error = ifnet_set_nat64prefix(ifp, ifnat64->ifnat64_prefixes);
		if (error != 0) {
			ip6stat.ip6s_clat464_plat64_pfx_setfail++;
		}
		break;

	case SIOCGIFNAT64PREFIX:                /* struct if_nat64req */
		error = ifnet_get_nat64prefix(ifp, ifnat64->ifnat64_prefixes);
		if (error != 0) {
			ip6stat.ip6s_clat464_plat64_pfx_getfail++;
		}
		break;

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

static __attribute__((noinline)) int
ifioctl_clat46addr(struct ifnet *ifp, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data)
{
	struct if_clat46req *ifclat46 = (struct if_clat46req *)(void *)data;
	struct in6_ifaddr *ia6_clat = NULL;
	int error = 0;

	VERIFY(ifp != NULL);

	switch (cmd) {
	case SIOCGIFCLAT46ADDR:
		ia6_clat = in6ifa_ifpwithflag(ifp, IN6_IFF_CLAT46);
		if (ia6_clat == NULL) {
			error = ENOENT;
			break;
		}

		bcopy(&ia6_clat->ia_addr.sin6_addr, &ifclat46->ifclat46_addr.v6_address,
		    sizeof(ifclat46->ifclat46_addr.v6_address));
		ifclat46->ifclat46_addr.v6_prefixlen = ia6_clat->ia_plen;
		ifa_remref(&ia6_clat->ia_ifa);
		break;
	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

#if SKYWALK
static __attribute__((noinline)) int
ifioctl_nexus(struct ifnet *ifp, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data)
{
	int error = 0;
	struct if_nexusreq *ifnr = (struct if_nexusreq *)(void *)data;

	switch (cmd) {
	case SIOCGIFNEXUS:              /* struct if_nexusreq */
		if (ifnr->ifnr_flags != 0) {
			error = EINVAL;
			break;
		}
		error = kern_nexus_get_netif_instance(ifp, ifnr->ifnr_netif);
		if (error != 0) {
			break;
		}
		kern_nexus_get_flowswitch_instance(ifp, ifnr->ifnr_flowswitch);
		break;
	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}
#endif /* SKYWALK */

static int
ifioctl_get_protolist(struct ifnet *ifp, u_int32_t * ret_count,
    user_addr_t ifpl)
{
	u_int32_t       actual_count;
	u_int32_t       count;
	int             error = 0;
	u_int32_t       *list = NULL;

	/* find out how many */
	count = if_get_protolist(ifp, NULL, 0);
	if (ifpl == USER_ADDR_NULL) {
		goto done;
	}

	/* copy out how many there's space for */
	if (*ret_count < count) {
		count = *ret_count;
	}
	if (count == 0) {
		goto done;
	}
	list = (u_int32_t *)kalloc_data(count * sizeof(*list), Z_WAITOK | Z_ZERO);
	if (list == NULL) {
		error = ENOMEM;
		goto done;
	}
	actual_count = if_get_protolist(ifp, list, count);
	if (actual_count < count) {
		count = actual_count;
	}
	if (count != 0) {
		error = copyout((caddr_t)list, ifpl, count * sizeof(*list));
	}

done:
	if (list != NULL) {
		if_free_protolist(list);
	}
	*ret_count = count;
	return error;
}

static __attribute__((noinline)) int
ifioctl_protolist(struct ifnet *ifp, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data)
{
	int error = 0;

	switch (cmd) {
	case SIOCGIFPROTOLIST32: {              /* struct if_protolistreq32 */
		struct if_protolistreq32        ifpl;

		bcopy(data, &ifpl, sizeof(ifpl));
		if (ifpl.ifpl_reserved != 0) {
			error = EINVAL;
			break;
		}
		error = ifioctl_get_protolist(ifp, &ifpl.ifpl_count,
		    CAST_USER_ADDR_T(ifpl.ifpl_list));
		bcopy(&ifpl, data, sizeof(ifpl));
		break;
	}
	case SIOCGIFPROTOLIST64: {              /* struct if_protolistreq64 */
		struct if_protolistreq64        ifpl;

		bcopy(data, &ifpl, sizeof(ifpl));
		if (ifpl.ifpl_reserved != 0) {
			error = EINVAL;
			break;
		}
		error = ifioctl_get_protolist(ifp, &ifpl.ifpl_count,
		    CAST_USER_ADDR_T(ifpl.ifpl_list));
		bcopy(&ifpl, data, sizeof(ifpl));
		break;
	}
	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

/*
 * List the ioctl()s we can perform on restricted INTCOPROC interfaces.
 */
static bool
ifioctl_restrict_intcoproc(unsigned long cmd, const char *__null_terminated ifname,
    struct ifnet *ifp, struct proc *p)
{
	if (intcoproc_unrestricted) {
		return false;
	}
	if (proc_pid(p) == 0) {
		return false;
	}
	if (ifname) {
		ifp = ifunit(ifname);
	}
	if (ifp == NULL) {
		return false;
	}
	if (!IFNET_IS_INTCOPROC(ifp)) {
		return false;
	}
	switch (cmd) {
	case SIOCGIFBRDADDR:
	case SIOCGIFCONF32:
	case SIOCGIFCONF64:
	case SIOCGIFFLAGS:
	case SIOCGIFEFLAGS:
	case SIOCGIFCAP:
	case SIOCGIFMETRIC:
	case SIOCGIFMTU:
	case SIOCGIFPHYS:
	case SIOCGIFTYPE:
	case SIOCGIFFUNCTIONALTYPE:
	case SIOCGIFPSRCADDR:
	case SIOCGIFPDSTADDR:
	case SIOCGIFGENERIC:
	case SIOCGIFDEVMTU:
	case SIOCGIFVLAN:
	case SIOCGIFBOND:
	case SIOCGIFWAKEFLAGS:
	case SIOCGIFGETRTREFCNT:
	case SIOCGIFOPPORTUNISTIC:
	case SIOCGIFLINKQUALITYMETRIC:
	case SIOCGIFLOG:
	case SIOCGIFDELEGATE:
	case SIOCGIFEXPENSIVE:
	case SIOCGIFINTERFACESTATE:
	case SIOCGIFPROBECONNECTIVITY:
	case SIOCGIFTIMESTAMPENABLED:
	case SIOCGECNMODE:
	case SIOCGQOSMARKINGMODE:
	case SIOCGQOSMARKINGENABLED:
	case SIOCGIFLOWINTERNET:
	case SIOCGIFSTATUS:
	case SIOCGIFMEDIA32:
	case SIOCGIFMEDIA64:
	case SIOCGIFXMEDIA32:
	case SIOCGIFXMEDIA64:
	case SIOCGIFDESC:
	case SIOCGIFLINKPARAMS:
	case SIOCGIFQUEUESTATS:
	case SIOCGIFTHROTTLE:
	case SIOCGIFAGENTIDS32:
	case SIOCGIFAGENTIDS64:
	case SIOCGIFNETSIGNATURE:
	case SIOCGIFINFO_IN6:
	case SIOCGIFAFLAG_IN6:
	case SIOCGNBRINFO_IN6:
	case SIOCGIFALIFETIME_IN6:
	case SIOCGIFNETMASK_IN6:
#if SKYWALK
	case SIOCGIFNEXUS:
#endif /* SKYWALK */
	case SIOCGIFPROTOLIST32:
	case SIOCGIFPROTOLIST64:
	case SIOCGIFXFLAGS:
	case SIOCGIFNOTRAFFICSHAPING:
	case SIOCGIFGENERATIONID:
	case SIOCSIFDIRECTLINK:
	case SIOCGIFDIRECTLINK:
		return false;
	default:
#if (DEBUG || DEVELOPMENT)
		printf("%s: cmd 0x%lx not allowed (pid %u)\n",
		    __func__, cmd, proc_pid(p));
#endif
		return true;
	}
	return false;
}

static bool
ifioctl_restrict_management(unsigned long cmd, const char *__null_terminated ifname,
    struct ifnet *ifp, struct proc *p)
{
	if (if_management_interface_check_needed == false) {
		return false;
	}
	if (management_control_unrestricted) {
		return false;
	}
	if (proc_pid(p) == 0) {
		return false;
	}
	if (ifname) {
		ifp = ifunit(ifname);
	}
	if (ifp == NULL) {
		return false;
	}
	if (!IFNET_IS_MANAGEMENT(ifp)) {
		return false;
	}
	/*
	 * Allow all the "get" ioctls
	 */
	switch (cmd) {
	case SIOCGHIWAT:
	case SIOCGLOWAT:
	case SIOCGPGRP:
	case SIOCGIFFLAGS:
	case SIOCGIFMETRIC:
	case SIOCGIFADDR:
	case SIOCGIFDSTADDR:
	case SIOCGIFBRDADDR:
	case SIOCGIFCONF32:
	case SIOCGIFCONF64:
	case SIOCGIFNETMASK:
	case SIOCGIFMTU:
	case SIOCGIFPHYS:
	case SIOCGIFMEDIA32:
	case SIOCGIFMEDIA64:
	case SIOCGIFGENERIC:
	case SIOCGIFSTATUS:
	case SIOCGIFPSRCADDR:
	case SIOCGIFPDSTADDR:
	case SIOCGIFDEVMTU:
	case SIOCGIFALTMTU:
	case SIOCGIFBOND:
	case SIOCGIFXMEDIA32:
	case SIOCGIFXMEDIA64:
	case SIOCGIFCAP:
	case SIOCGDRVSPEC32:
	case SIOCGDRVSPEC64:
	case SIOCGIFVLAN:
	case SIOCGIFASYNCMAP:
	case SIOCGIFMAC:
	case SIOCGIFKPI:
	case SIOCGIFWAKEFLAGS:
	case SIOCGIFGETRTREFCNT:
	case SIOCGIFLINKQUALITYMETRIC:
	case SIOCGIFOPPORTUNISTIC:
	case SIOCGIFEFLAGS:
	case SIOCGIFDESC:
	case SIOCGIFLINKPARAMS:
	case SIOCGIFQUEUESTATS:
	case SIOCGIFTHROTTLE:
	case SIOCGASSOCIDS32:
	case SIOCGASSOCIDS64:
	case SIOCGCONNIDS32:
	case SIOCGCONNIDS64:
	case SIOCGCONNINFO32:
	case SIOCGCONNINFO64:
	case SIOCGCONNORDER:
	case SIOCGIFLOG:
	case SIOCGIFDELEGATE:
	case SIOCGIFLLADDR:
	case SIOCGIFTYPE:
	case SIOCGIFEXPENSIVE:
	case SIOCGIF2KCL:
	case SIOCGSTARTDELAY:
	case SIOCGIFAGENTIDS32:
	case SIOCGIFAGENTIDS64:
	case SIOCGIFAGENTDATA32:
	case SIOCGIFAGENTDATA64:
	case SIOCGIFINTERFACESTATE:
	case SIOCGIFPROBECONNECTIVITY:
	case SIOCGIFFUNCTIONALTYPE:
	case SIOCGIFNETSIGNATURE:
	case SIOCGECNMODE:
	case SIOCGIFORDER:
	case SIOCGQOSMARKINGMODE:
	case SIOCGQOSMARKINGENABLED:
	case SIOCGIFTIMESTAMPENABLED:
	case SIOCGIFAGENTLIST32:
	case SIOCGIFAGENTLIST64:
	case SIOCGIFLOWINTERNET:
	case SIOCGIFNAT64PREFIX:
#if SKYWALK
	case SIOCGIFNEXUS:
#endif /* SKYWALK */
	case SIOCGIFPROTOLIST32:
	case SIOCGIFPROTOLIST64:
	case SIOCGIF6LOWPAN:
	case SIOCGIFTCPKAOMAX:
	case SIOCGIFLOWPOWER:
	case SIOCGIFCLAT46ADDR:
	case SIOCGIFMPKLOG:
	case SIOCGIFCONSTRAINED:
	case SIOCGIFULTRACONSTRAINED:
	case SIOCGIFXFLAGS:
	case SIOCGIFNOACKPRIO:
	case SIOCGETROUTERMODE:
	case SIOCGIFNOTRAFFICSHAPING:
	case SIOCGIFGENERATIONID:
	case SIOCSIFDIRECTLINK:
	case SIOCGIFDIRECTLINK:
	case SIOCSIFISVPN:
	case SIOCSIFDELAYWAKEPKTEVENT:
	case SIOCGIFDELAYWAKEPKTEVENT:
	case SIOCGIFDISABLEINPUT:
		return false;
	default:
		if (!IOCurrentTaskHasEntitlement(MANAGEMENT_CONTROL_ENTITLEMENT)) {
#if (DEBUG || DEVELOPMENT)
			printf("ifioctl_restrict_management: cmd 0x%lx on %s not allowed for %s:%u\n",
			    cmd, ifname, proc_name_address(p), proc_pid(p));
#endif
			return true;
		}
		return false;
	}
	return false;
}

/*
 * Given a media word, return one suitable for an application
 * using the original encoding.
 */
static int
compat_media(int media)
{
	if (IFM_TYPE(media) == IFM_ETHER && IFM_SUBTYPE(media) > IFM_OTHER) {
		media &= ~IFM_TMASK;
		media |= IFM_OTHER;
	}
	return media;
}

static int
compat_ifmu_ulist(struct ifnet *ifp, u_long cmd, void *__sized_by(IOCPARM_LEN(cmd)) data)
{
	// cast to 32bit version to work within bounds with 32bit userspace
	struct ifmediareq32 *ifmr = (struct ifmediareq32 *)data;
	user_addr_t user_addr;
	int i;
	int *media_list = NULL;
	int error = 0;
	bool list_modified = false;

	user_addr = (cmd == SIOCGIFMEDIA64) ?
	    CAST_USER_ADDR_T(((struct ifmediareq64 *)data)->ifmu_ulist) :
	    CAST_USER_ADDR_T(((struct ifmediareq32 *)data)->ifmu_ulist);
	if (user_addr == USER_ADDR_NULL || ifmr->ifm_count == 0) {
		return 0;
	}
	media_list = (int *)kalloc_data(ifmr->ifm_count * sizeof(int),
	    Z_WAITOK | Z_ZERO);
	if (media_list == NULL) {
		os_log_error(OS_LOG_DEFAULT,
		    "%s: %s kalloc_data() failed",
		    __func__, ifp->if_xname);
		error = ENOMEM;
		goto done;
	}
	error = copyin(user_addr, media_list, ifmr->ifm_count * sizeof(int));
	if (error != 0) {
		os_log_error(OS_LOG_DEFAULT,
		    "%s: %s copyin() error %d",
		    __func__, ifp->if_xname, error);
		goto done;
	}
	for (i = 0; i < ifmr->ifm_count; i++) {
		int old_media, new_media;

		old_media = media_list[i];

		new_media = compat_media(old_media);
		if (new_media == old_media) {
			continue;
		}
		if (if_verbose != 0) {
			os_log_info(OS_LOG_DEFAULT,
			    "%s: %s converted extended media %08x to compat media %08x",
			    __func__, ifp->if_xname, old_media, new_media);
		}
		media_list[i] = new_media;
		list_modified = true;
	}
	if (list_modified) {
		error = copyout(media_list, user_addr, ifmr->ifm_count * sizeof(int));
		if (error != 0) {
			os_log_error(OS_LOG_DEFAULT,
			    "%s: %s copyout() error %d",
			    __func__, ifp->if_xname, error);
			goto done;
		}
	}
done:
	if (media_list != NULL) {
		kfree_data(media_list, ifmr->ifm_count * sizeof(int));
	}
	return error;
}

static int
compat_ifmediareq(struct ifnet *ifp, u_long cmd, void *__sized_by(IOCPARM_LEN(cmd)) data)
{
	// cast to 32bit version to work within bounds with 32bit userspace
	struct ifmediareq32 *ifmr = (struct ifmediareq32 *)data;
	int error;

	ifmr->ifm_active = compat_media(ifmr->ifm_active);
	ifmr->ifm_current = compat_media(ifmr->ifm_current);

	error = compat_ifmu_ulist(ifp, cmd, data);

	return error;
}

static int
ifioctl_get_media(struct ifnet *ifp, struct socket *so, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data)
{
	int error = 0;

	/*
	 * An ifnet must not implement SIOCGIFXMEDIA as it gets the extended
	 *  media subtypes macros from <net/if_media.h>
	 */
	switch (cmd) {
	case SIOCGIFMEDIA32:
	case SIOCGIFXMEDIA32:
		error = ifnet_ioctl(ifp, SOCK_DOM(so), SIOCGIFMEDIA32, data);
		break;
	case SIOCGIFMEDIA64:
	case SIOCGIFXMEDIA64:
		error = ifnet_ioctl(ifp, SOCK_DOM(so), SIOCGIFMEDIA64, data);
		break;
	}
	if (if_verbose != 0 && error != 0) {
		os_log(OS_LOG_DEFAULT, "%s: first ifnet_ioctl(%s, %08lx) error %d",
		    __func__, ifp->if_xname, cmd, error);
	}
	if (error == 0 && (cmd == SIOCGIFMEDIA32 || cmd == SIOCGIFMEDIA64)) {
		error = compat_ifmediareq(ifp, cmd, data);
	}
	return error;
}

static errno_t
null_proto_input(ifnet_t ifp, protocol_family_t protocol, mbuf_t packet,
    char *header)
{
#pragma unused(protocol, packet, header)
	os_log(OS_LOG_DEFAULT, "null_proto_input unexpected packet on %s",
	    ifp->if_xname);
	return 0;
}

/*
 * Interface ioctls.
 *
 * Most of the routines called to handle the ioctls would end up being
 * tail-call optimized, which unfortunately causes this routine to
 * consume too much stack space; this is the reason for the "noinline"
 * attribute used on those routines.
 */
int
ifioctl(struct socket *so, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data, struct proc *p)
{
	char ifname[IFNAMSIZ + 1];
	struct ifnet *ifp = NULL;
	struct ifstat *ifs = NULL;
	int error = 0;

	bzero(ifname, sizeof(ifname));

	/*
	 * ioctls which don't require ifp, or ifreq ioctls
	 */
	switch (cmd) {
	case OSIOCGIFCONF32:                    /* struct ifconf32 */
	case SIOCGIFCONF32:                     /* struct ifconf32 */
	case SIOCGIFCONF64:                     /* struct ifconf64 */
	case OSIOCGIFCONF64:                    /* struct ifconf64 */
		error = ifioctl_ifconf(cmd, data);
		goto done;

	case SIOCIFGCLONERS32:                  /* struct if_clonereq32 */
	case SIOCIFGCLONERS64:                  /* struct if_clonereq64 */
		error = ifioctl_ifclone(cmd, data);
		goto done;

	case SIOCGIFAGENTDATA32:                /* struct netagent_req32 */
	case SIOCGIFAGENTDATA64:                /* struct netagent_req64 */
	case SIOCGIFAGENTLIST32:                /* struct netagentlist_req32 */
	case SIOCGIFAGENTLIST64:                /* struct netagentlist_req64 */
		error = netagent_ioctl(cmd, data);
		goto done;

	case SIOCSIFORDER:                      /* struct if_order */
	case SIOCGIFORDER:                      /* struct if_order */
		error = ifioctl_iforder(cmd, data);
		goto done;

	case SIOCSIFDSTADDR:                    /* struct ifreq */
	case SIOCSIFADDR:                       /* struct ifreq */
	case SIOCSIFBRDADDR:                    /* struct ifreq */
	case SIOCSIFNETMASK:                    /* struct ifreq */
	case OSIOCGIFADDR:                      /* struct ifreq */
	case OSIOCGIFDSTADDR:                   /* struct ifreq */
	case OSIOCGIFBRDADDR:                   /* struct ifreq */
	case OSIOCGIFNETMASK:                   /* struct ifreq */
	case SIOCSIFKPI:                        /* struct ifreq */
		if (so->so_proto == NULL) {
			error = EOPNOTSUPP;
			goto done;
		}
		OS_FALLTHROUGH;
	case SIOCIFCREATE:                      /* struct ifreq */
	case SIOCIFCREATE2:                     /* struct ifreq */
	case SIOCIFDESTROY:                     /* struct ifreq */
	case SIOCGIFFLAGS:                      /* struct ifreq */
	case SIOCGIFEFLAGS:                     /* struct ifreq */
	case SIOCGIFCAP:                        /* struct ifreq */
	case SIOCGIFMETRIC:                     /* struct ifreq */
	case SIOCGIFMTU:                        /* struct ifreq */
	case SIOCGIFPHYS:                       /* struct ifreq */
	case SIOCSIFFLAGS:                      /* struct ifreq */
	case SIOCSIFCAP:                        /* struct ifreq */
	case SIOCSIFMANAGEMENT:                 /* struct ifreq */
	case SIOCSATTACHPROTONULL:              /* struct ifreq */
	case SIOCSIFMETRIC:                     /* struct ifreq */
	case SIOCSIFPHYS:                       /* struct ifreq */
	case SIOCSIFMTU:                        /* struct ifreq */
	case SIOCADDMULTI:                      /* struct ifreq */
	case SIOCDELMULTI:                      /* struct ifreq */
	case SIOCDIFPHYADDR:                    /* struct ifreq */
	case SIOCSIFMEDIA:                      /* struct ifreq */
	case SIOCSIFGENERIC:                    /* struct ifreq */
	case SIOCSIFLLADDR:                     /* struct ifreq */
	case SIOCSIFALTMTU:                     /* struct ifreq */
	case SIOCSIFVLAN:                       /* struct ifreq */
	case SIOCSIFBOND:                       /* struct ifreq */
	case SIOCGIFLLADDR:                     /* struct ifreq */
	case SIOCGIFTYPE:                       /* struct ifreq */
	case SIOCGIFFUNCTIONALTYPE:             /* struct ifreq */
	case SIOCGIFPSRCADDR:                   /* struct ifreq */
	case SIOCGIFPDSTADDR:                   /* struct ifreq */
	case SIOCGIFGENERIC:                    /* struct ifreq */
	case SIOCGIFDEVMTU:                     /* struct ifreq */
	case SIOCGIFVLAN:                       /* struct ifreq */
	case SIOCGIFBOND:                       /* struct ifreq */
	case SIOCGIFWAKEFLAGS:                  /* struct ifreq */
	case SIOCGIFGETRTREFCNT:                /* struct ifreq */
	case SIOCSIFOPPORTUNISTIC:              /* struct ifreq */
	case SIOCGIFOPPORTUNISTIC:              /* struct ifreq */
	case SIOCGIFLINKQUALITYMETRIC:          /* struct ifreq */
	case SIOCSIFLINKQUALITYMETRIC:          /* struct ifreq */
	case SIOCSIFLOG:                        /* struct ifreq */
	case SIOCGIFLOG:                        /* struct ifreq */
	case SIOCGIFDELEGATE:                   /* struct ifreq */
	case SIOCGIFEXPENSIVE:                  /* struct ifreq */
	case SIOCSIFEXPENSIVE:                  /* struct ifreq */
	case SIOCSIF2KCL:                       /* struct ifreq */
	case SIOCGIF2KCL:                       /* struct ifreq */
	case SIOCSIFINTERFACESTATE:             /* struct ifreq */
	case SIOCGIFINTERFACESTATE:             /* struct ifreq */
	case SIOCSIFPROBECONNECTIVITY:          /* struct ifreq */
	case SIOCGIFPROBECONNECTIVITY:          /* struct ifreq */
	case SIOCGSTARTDELAY:                   /* struct ifreq */
	case SIOCSIFTIMESTAMPENABLE:            /* struct ifreq */
	case SIOCSIFTIMESTAMPDISABLE:           /* struct ifreq */
	case SIOCGIFTIMESTAMPENABLED:           /* struct ifreq */
#if (DEBUG || DEVELOPMENT)
	case SIOCSIFDISABLEOUTPUT:              /* struct ifreq */
#endif /* (DEBUG || DEVELOPMENT) */
	case SIOCSIFSUBFAMILY:                  /* struct ifreq */
	case SIOCGECNMODE:                      /* struct ifreq */
	case SIOCSECNMODE:
	case SIOCSQOSMARKINGMODE:               /* struct ifreq */
	case SIOCSQOSMARKINGENABLED:            /* struct ifreq */
	case SIOCGQOSMARKINGMODE:               /* struct ifreq */
	case SIOCGQOSMARKINGENABLED:            /* struct ifreq */
	case SIOCSIFLOWINTERNET:                /* struct ifreq */
	case SIOCGIFLOWINTERNET:                /* struct ifreq */
	case SIOCGIFLOWPOWER:                   /* struct ifreq */
	case SIOCSIFLOWPOWER:                   /* struct ifreq */
	case SIOCGIFMPKLOG:                     /* struct ifreq */
	case SIOCSIFMPKLOG:                     /* struct ifreq */
	case SIOCGIFCONSTRAINED:                /* struct ifreq */
	case SIOCSIFCONSTRAINED:                /* struct ifreq */
	case SIOCGIFULTRACONSTRAINED:           /* struct ifreq */
	case SIOCSIFULTRACONSTRAINED:           /* struct ifreq */
	case SIOCSIFESTTHROUGHPUT:              /* struct ifreq */
	case SIOCSIFRADIODETAILS:               /* struct ifreq */
	case SIOCGIFXFLAGS:                     /* struct ifreq */
	case SIOCGIFNOACKPRIO:                  /* struct ifreq */
	case SIOCSIFNOACKPRIO:                  /* struct ifreq */
	case SIOCSIFMARKWAKEPKT:                /* struct ifreq */
	case SIOCSIFNOTRAFFICSHAPING:           /* struct ifreq */
	case SIOCGIFNOTRAFFICSHAPING:           /* struct ifreq */
	case SIOCGIFGENERATIONID:               /* struct ifreq */
	case SIOCSIFDIRECTLINK:                 /* struct ifreq */
	case SIOCGIFDIRECTLINK:                 /* struct ifreq */
	case SIOCSIFISVPN:                      /* struct ifreq */
	case SIOCSIFDELAYWAKEPKTEVENT:          /* struct ifreq */
	case SIOCGIFDELAYWAKEPKTEVENT:          /* struct ifreq */
	case SIOCSIFDISABLEINPUT:               /* struct ifreq */
	case SIOCGIFDISABLEINPUT:               /* struct ifreq */
	{
		struct ifreq ifr;
		bcopy(data, &ifr, sizeof(ifr));
		ifr.ifr_name[IFNAMSIZ - 1] = '\0';
		bcopy(&ifr.ifr_name, ifname, IFNAMSIZ);
		if (ifioctl_restrict_intcoproc(cmd, __unsafe_null_terminated_from_indexable(ifname), NULL, p) == true) {
			error = EPERM;
			goto done;
		}
		if (ifioctl_restrict_management(cmd, __unsafe_null_terminated_from_indexable(ifname), NULL, p) == true) {
			error = EPERM;
			goto done;
		}
		error = ifioctl_ifreq(so, cmd, &ifr, p);
		bcopy(&ifr, data, sizeof(ifr));
		goto done;
	}
	}

	/*
	 * ioctls which require ifp.  Note that we acquire dlil_ifnet_lock
	 * here to ensure that the ifnet, if found, has been fully attached.
	 */
	dlil_if_lock();
	switch (cmd) {
	case SIOCSIFPHYADDR:                    /* struct {if,in_}aliasreq */
		bcopy(((struct in_aliasreq *)(void *)data)->ifra_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCSIFPHYADDR_IN6_32:             /* struct in6_aliasreq_32 */
		bcopy(((struct in6_aliasreq_32 *)(void *)data)->ifra_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCSIFPHYADDR_IN6_64:             /* struct in6_aliasreq_64 */
		bcopy(((struct in6_aliasreq_64 *)(void *)data)->ifra_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCGIFSTATUS:                     /* struct ifstat */
		ifs = kalloc_type(struct ifstat, Z_WAITOK | Z_NOFAIL);
		bcopy(data, ifs, sizeof(*ifs));
		ifs->ifs_name[IFNAMSIZ - 1] = '\0';
		bcopy(ifs->ifs_name, ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCGIFMEDIA32:                    /* struct ifmediareq32 */
	case SIOCGIFXMEDIA32:                   /* struct ifmediareq32 */
		bcopy(((struct ifmediareq32 *)(void *)data)->ifm_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCGIFMEDIA64:                    /* struct ifmediareq64 */
	case SIOCGIFXMEDIA64:                   /* struct ifmediareq64 */
		bcopy(((struct ifmediareq64 *)(void *)data)->ifm_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCSIFDESC:                       /* struct if_descreq */
	case SIOCGIFDESC:                       /* struct if_descreq */
		bcopy(((struct if_descreq *)(void *)data)->ifdr_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCSIFLINKPARAMS:                 /* struct if_linkparamsreq */
	case SIOCGIFLINKPARAMS:                 /* struct if_linkparamsreq */
		bcopy(((struct if_linkparamsreq *)(void *)data)->iflpr_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCGIFQUEUESTATS:                 /* struct if_qstatsreq */
		bcopy(((struct if_qstatsreq *)(void *)data)->ifqr_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCSIFTHROTTLE:                   /* struct if_throttlereq */
	case SIOCGIFTHROTTLE:                   /* struct if_throttlereq */
		bcopy(((struct if_throttlereq *)(void *)data)->ifthr_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCGIFAGENTIDS32:                 /* struct if_agentidsreq32 */
		bcopy(((struct if_agentidsreq32 *)(void *)data)->ifar_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCGIFAGENTIDS64:                 /* struct if_agentidsreq64 */
		bcopy(((struct if_agentidsreq64 *)(void *)data)->ifar_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCAIFAGENTID:                    /* struct if_agentidreq */
	case SIOCDIFAGENTID:                    /* struct if_agentidreq */
		bcopy(((struct if_agentidreq *)(void *)data)->ifar_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCSIFNETSIGNATURE:               /* struct if_nsreq */
	case SIOCGIFNETSIGNATURE:               /* struct if_nsreq */
		bcopy(((struct if_nsreq *)(void *)data)->ifnsr_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCSIFNETWORKID:                  /* struct if_netidreq */
		bcopy(((struct if_netidreq *)(void *)data)->ifnetid_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;
#if SKYWALK
	case SIOCGIFNEXUS:                      /* struct if_nexusreq */
		bcopy(((struct if_nexusreq *)(void *)data)->ifnr_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;
#endif /* SKYWALK */
	case SIOCGIFPROTOLIST32:                /* struct if_protolistreq32 */
		bcopy(((struct if_protolistreq32 *)(void *)data)->ifpl_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCGIFPROTOLIST64:                /* struct if_protolistreq64 */
		bcopy(((struct if_protolistreq64 *)(void *)data)->ifpl_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCSDEFIFACE_IN6_32:              /* struct in6_ndifreq_32 */
		bcopy(((struct in6_ndifreq_32 *)(void *)data)->ifname,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCSDEFIFACE_IN6_64:              /* struct in6_ndifreq_64 */
		bcopy(((struct in6_ndifreq_64 *)(void *)data)->ifname,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCLL_CGASTART_32:            /* struct in6_cgareq_32 */
	case SIOCSIFCGAPREP_IN6_32:         /* struct in6_cgareq_32 */
		bcopy(((struct in6_cgareq_32 *)(void *)data)->cgar_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCLL_CGASTART_64:            /* struct in6_cgareq_64 */
	case SIOCSIFCGAPREP_IN6_64:         /* struct in6_cgareq_64 */
		bcopy(((struct in6_cgareq_64 *)(void *)data)->cgar_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCGIFINFO_IN6:               /* struct in6_ondireq */
		bcopy(((struct in6_ondireq *)(void *)data)->ifname,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCGIFCLAT46ADDR:            /* struct if_clat46req */
		bcopy(((struct if_clat46req *)(void *)data)->ifclat46_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCGIFNAT64PREFIX:            /* struct if_nat64req */
		bcopy(((struct if_nat64req *)(void *)data)->ifnat64_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCGIFAFLAG_IN6:              /* struct in6_ifreq */
	case SIOCAUTOCONF_STOP:             /* struct in6_ifreq */
	case SIOCGIFALIFETIME_IN6:          /* struct in6_ifreq */
	case SIOCAUTOCONF_START:            /* struct in6_ifreq */
	case SIOCGIFPSRCADDR_IN6:           /* struct in6_ifreq */
	case SIOCPROTODETACH_IN6:           /* struct in6_ifreq */
	case SIOCSPFXFLUSH_IN6:             /* struct in6_ifreq */
	case SIOCSRTRFLUSH_IN6:             /* struct in6_ifreq */
		bcopy(((struct in6_ifreq *)(void *)data)->ifr_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;

	case SIOCPROTOATTACH:                /* struct ifreq */
	case SIOCLL_STOP:                    /* struct ifreq */
	case SIOCAUTOADDR:                   /* struct ifreq */
	case SIOCDIFADDR:                    /* struct ifreq */
	case SIOCARPIPLL:                    /* struct ifreq */
	case SIOCGIFADDR:                    /* struct ifreq */
		bcopy(((struct ifreq *)(void *)data)->ifr_name,
		    ifname, IFNAMSIZ);
		ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		break;
	default:
	{
		size_t data_size = IOCPARM_LEN(cmd);
		size_t ifr_name_off = offsetof(struct ifreq, ifr_name);
		if (data_size > ifr_name_off) {
			strbufcpy(ifname, sizeof(ifname), data + ifr_name_off, data_size - ifr_name_off);
			ifp = ifunit_ref(__unsafe_null_terminated_from_indexable(ifname));
		} else {
			error = EINVAL;
		}
	}
	break;
	}
	dlil_if_unlock();

	if (ifp == NULL) {
		error = ENXIO;
		goto done;
	}

	if (ifioctl_restrict_intcoproc(cmd, NULL, ifp, p) == true) {
		error = EPERM;
		goto done;
	}
	switch (cmd) {
	case SIOCSIFPHYADDR:                    /* struct {if,in_}aliasreq */
	case SIOCSIFPHYADDR_IN6_32:             /* struct in6_aliasreq_32 */
	case SIOCSIFPHYADDR_IN6_64:             /* struct in6_aliasreq_64 */
		error = proc_suser(p);
		if (error != 0) {
			break;
		}

		error = ifnet_ioctl(ifp, SOCK_DOM(so), cmd, data);
		if (error != 0) {
			break;
		}

		ifnet_touch_lastchange(ifp);
		break;

	case SIOCGIFSTATUS:                     /* struct ifstat */
		VERIFY(ifs != NULL);
		ifs->ascii[0] = '\0';

		error = ifnet_ioctl(ifp, SOCK_DOM(so), cmd, (caddr_t)ifs);

		bcopy(ifs, data, sizeof(*ifs));
		break;

	case SIOCGIFMEDIA32:                    /* struct ifmediareq32 */
	case SIOCGIFMEDIA64:                    /* struct ifmediareq64 */
	case SIOCGIFXMEDIA32:                    /* struct ifmediareq32 */
	case SIOCGIFXMEDIA64:                    /* struct ifmediareq64 */
		error = ifioctl_get_media(ifp, so, cmd, data);
		break;

	case SIOCSIFDESC:                       /* struct if_descreq */
	case SIOCGIFDESC:                       /* struct if_descreq */
		error = ifioctl_ifdesc(ifp, cmd, data, p);
		break;

	case SIOCSIFLINKPARAMS:                 /* struct if_linkparamsreq */
	case SIOCGIFLINKPARAMS:                 /* struct if_linkparamsreq */
		error = ifioctl_linkparams(ifp, cmd, data, p);
		break;

	case SIOCGIFQUEUESTATS:                 /* struct if_qstatsreq */
		error = ifioctl_qstats(ifp, cmd, data);
		break;

	case SIOCSIFTHROTTLE:                   /* struct if_throttlereq */
	case SIOCGIFTHROTTLE:                   /* struct if_throttlereq */
		error = ifioctl_throttle(ifp, cmd, data, p);
		break;

	case SIOCAIFAGENTID:                    /* struct if_agentidreq */
	case SIOCDIFAGENTID:                    /* struct if_agentidreq */
	case SIOCGIFAGENTIDS32:                 /* struct if_agentidsreq32 */
	case SIOCGIFAGENTIDS64:                 /* struct if_agentidsreq64 */
		error = ifioctl_netagent(ifp, cmd, data, p);
		break;

	case SIOCSIFNETSIGNATURE:               /* struct if_nsreq */
	case SIOCGIFNETSIGNATURE:               /* struct if_nsreq */
		error = ifioctl_netsignature(ifp, cmd, data);
		break;

	case SIOCSIFNETWORKID:                  /* struct if_netidreq */
		error = ifioctl_networkid(ifp, data);
		break;
	case SIOCSIFNAT64PREFIX:                /* struct if_nat64req */
	case SIOCGIFNAT64PREFIX:                /* struct if_nat64req */
		error = ifioctl_nat64prefix(ifp, cmd, data);
		break;

	case SIOCGIFCLAT46ADDR:                 /* struct if_clat46req */
		error = ifioctl_clat46addr(ifp, cmd, data);
		break;
#if SKYWALK
	case SIOCGIFNEXUS:
		error = ifioctl_nexus(ifp, cmd, data);
		break;
#endif /* SKYWALK */

	case SIOCGIFPROTOLIST32:                /* struct if_protolistreq32 */
	case SIOCGIFPROTOLIST64:                /* struct if_protolistreq64 */
		error = ifioctl_protolist(ifp, cmd, data);
		break;

	default:
		if (so->so_proto == NULL) {
			error = EOPNOTSUPP;
			break;
		}

		socket_lock(so, 1);
		error = ((*so->so_proto->pr_usrreqs->pru_control)(so, cmd,
		    data, ifp, p));
		socket_unlock(so, 1);

		// Don't allow to call SIOCAIFADDR and SIOCDIFADDR with
		// ifreq as the code expects ifaddr
		if ((error == EOPNOTSUPP || error == ENOTSUP) &&
		    !(cmd == SIOCAIFADDR || cmd == SIOCDIFADDR)) {
			error = ifnet_ioctl(ifp, SOCK_DOM(so), cmd, data);
		}
		break;
	}

done:
	if (ifs != NULL) {
		kfree_type(struct ifstat, ifs);
	}

	if (if_verbose) {
		if (ifname[0] == '\0') {
			(void) snprintf(ifname, sizeof(ifname), "%s",
			    "NULL");
		} else if (ifp != NULL) {
			(void) snprintf(ifname, sizeof(ifname), "%s",
			    if_name(ifp));
		}

		if (error != 0) {
			printf("%s[%s,%d]: ifp %s cmd 0x%08lx (%c%c [%lu] "
			    "%c %lu) error %d\n", __func__,
			    proc_name_address(p), proc_pid(p),
			    ifname, cmd, (cmd & IOC_IN) ? 'I' : ' ',
			    (cmd & IOC_OUT) ? 'O' : ' ', IOCPARM_LEN(cmd),
			    (char)IOCGROUP(cmd), cmd & 0xff, error);
		} else if (if_verbose > 1) {
			printf("%s[%s,%d]: ifp %s cmd 0x%08lx (%c%c [%lu] "
			    "%c %lu) OK\n", __func__,
			    proc_name_address(p), proc_pid(p),
			    ifname, cmd, (cmd & IOC_IN) ? 'I' : ' ',
			    (cmd & IOC_OUT) ? 'O' : ' ', IOCPARM_LEN(cmd),
			    (char)IOCGROUP(cmd), cmd & 0xff);
		}
	}

	if (ifp != NULL) {
		ifnet_decr_iorefcnt(ifp);
	}
	return error;
}

static __attribute__((noinline)) int
ifioctl_ifreq(struct socket *so, u_long cmd, struct ifreq *ifr, struct proc *p)
{
	struct ifnet *ifp;
	u_long ocmd = cmd;
	int error = 0;
	struct kev_msg ev_msg;
	struct net_event_data ev_data;

	bzero(&ev_data, sizeof(struct net_event_data));
	bzero(&ev_msg, sizeof(struct kev_msg));

	switch (cmd) {
	case SIOCIFCREATE:
	case SIOCIFCREATE2:
		error = proc_suser(p);
		if (error) {
			return error;
		}
		return if_clone_create(ifr->ifr_name, sizeof(ifr->ifr_name),
		           cmd == SIOCIFCREATE2 ? ifr->ifr_data : NULL);
	case SIOCIFDESTROY:
		error = proc_suser(p);
		if (error) {
			return error;
		}
		return if_clone_destroy(ifr->ifr_name, sizeof(ifr->ifr_name));
	}

	/*
	 * ioctls which require ifp.  Note that we acquire dlil_ifnet_lock
	 * here to ensure that the ifnet, if found, has been fully attached.
	 */
	dlil_if_lock();
	ifp = ifunit(__unsafe_null_terminated_from_indexable(ifr->ifr_name));
	dlil_if_unlock();

	if (ifp == NULL) {
		return ENXIO;
	}

	switch (cmd) {
	case SIOCGIFFLAGS:
		ifnet_lock_shared(ifp);
		ifr->ifr_flags = ifp->if_flags;
		ifnet_lock_done(ifp);
		break;

	case SIOCGIFEFLAGS:
		ifnet_lock_shared(ifp);
		ifr->ifr_eflags = ifp->if_eflags;
		ifnet_lock_done(ifp);
		break;

	case SIOCGIFXFLAGS:
		ifnet_lock_shared(ifp);
		ifr->ifr_xflags = ifp->if_xflags;
		ifnet_lock_done(ifp);
		break;

	case SIOCGIFCAP:
		ifnet_lock_shared(ifp);
		ifr->ifr_reqcap = ifp->if_capabilities;
		ifr->ifr_curcap = ifp->if_capenable;
		ifnet_lock_done(ifp);
		break;

	case SIOCGIFMETRIC:
		ifnet_lock_shared(ifp);
		ifr->ifr_metric = ifp->if_metric;
		ifnet_lock_done(ifp);
		break;

	case SIOCGIFMTU:
		ifnet_lock_shared(ifp);
		ifr->ifr_mtu = ifp->if_mtu;
		ifnet_lock_done(ifp);
		break;

	case SIOCGIFPHYS:
		ifnet_lock_shared(ifp);
		ifr->ifr_phys = ifp->if_physical;
		ifnet_lock_done(ifp);
		break;

	case SIOCSIFFLAGS:
		error = proc_suser(p);
		if (error != 0) {
			break;
		}

		(void) ifnet_set_flags(ifp, ifr->ifr_flags,
		    (u_int16_t)~IFF_CANTCHANGE);

		/*
		 * Note that we intentionally ignore any error from below
		 * for the SIOCSIFFLAGS case.
		 */
		(void) ifnet_ioctl(ifp, SOCK_DOM(so), cmd, (caddr_t)ifr);

		/*
		 * Send the event even upon error from the driver because
		 * we changed the flags.
		 */
		dlil_post_sifflags_msg(ifp);

		ifnet_touch_lastchange(ifp);
		break;

	case SIOCSIFCAP:
		error = proc_suser(p);
		if (error != 0) {
			break;
		}

		if ((ifr->ifr_reqcap & ~ifp->if_capabilities)) {
			error = EINVAL;
			break;
		}
		error = ifnet_ioctl(ifp, SOCK_DOM(so), cmd, (caddr_t)ifr);

		ifnet_touch_lastchange(ifp);
		break;

	case SIOCSIFMETRIC:
		error = proc_suser(p);
		if (error != 0) {
			break;
		}

		ifp->if_metric = ifr->ifr_metric;

		ev_msg.vendor_code    = KEV_VENDOR_APPLE;
		ev_msg.kev_class      = KEV_NETWORK_CLASS;
		ev_msg.kev_subclass   = KEV_DL_SUBCLASS;

		ev_msg.event_code = KEV_DL_SIFMETRICS;
		strlcpy(&ev_data.if_name[0], ifp->if_name, IFNAMSIZ);
		ev_data.if_family = ifp->if_family;
		ev_data.if_unit   = (u_int32_t) ifp->if_unit;
		ev_msg.dv[0].data_length = sizeof(struct net_event_data);
		ev_msg.dv[0].data_ptr    = &ev_data;

		ev_msg.dv[1].data_length = 0;
		dlil_post_complete_msg(ifp, &ev_msg);

		ifnet_touch_lastchange(ifp);
		break;

	case SIOCSIFPHYS:
		error = proc_suser(p);
		if (error != 0) {
			break;
		}

		error = ifnet_ioctl(ifp, SOCK_DOM(so), cmd, (caddr_t)ifr);
		if (error != 0) {
			break;
		}

		ev_msg.vendor_code    = KEV_VENDOR_APPLE;
		ev_msg.kev_class      = KEV_NETWORK_CLASS;
		ev_msg.kev_subclass   = KEV_DL_SUBCLASS;

		ev_msg.event_code = KEV_DL_SIFPHYS;
		strlcpy(&ev_data.if_name[0], ifp->if_name, IFNAMSIZ);
		ev_data.if_family = ifp->if_family;
		ev_data.if_unit   = (u_int32_t) ifp->if_unit;
		ev_msg.dv[0].data_length = sizeof(struct net_event_data);
		ev_msg.dv[0].data_ptr    = &ev_data;
		ev_msg.dv[1].data_length = 0;
		dlil_post_complete_msg(ifp, &ev_msg);

		ifnet_touch_lastchange(ifp);
		break;

	case SIOCSIFMTU: {
		u_int32_t oldmtu = ifp->if_mtu;
		struct ifclassq *ifq = ifp->if_snd;

		ASSERT(ifq != NULL);
		error = proc_suser(p);
		if (error != 0) {
			break;
		}

		if (ifp->if_ioctl == NULL) {
			error = EOPNOTSUPP;
			break;
		}
		if (ifr->ifr_mtu < IF_MINMTU || ifr->ifr_mtu > IF_MAXMTU) {
			error = EINVAL;
			break;
		}
		error = ifnet_ioctl(ifp, SOCK_DOM(so), cmd, (caddr_t)ifr);
		if (error != 0) {
			break;
		}

		ev_msg.vendor_code    = KEV_VENDOR_APPLE;
		ev_msg.kev_class      = KEV_NETWORK_CLASS;
		ev_msg.kev_subclass   = KEV_DL_SUBCLASS;

		ev_msg.event_code = KEV_DL_SIFMTU;
		strlcpy(&ev_data.if_name[0], ifp->if_name, IFNAMSIZ);
		ev_data.if_family = ifp->if_family;
		ev_data.if_unit   = (u_int32_t) ifp->if_unit;
		ev_msg.dv[0].data_length = sizeof(struct net_event_data);
		ev_msg.dv[0].data_ptr    = &ev_data;
		ev_msg.dv[1].data_length = 0;
		dlil_post_complete_msg(ifp, &ev_msg);

		ifnet_touch_lastchange(ifp);
		rt_ifmsg(ifp);

		/*
		 * If the link MTU changed, do network layer specific procedure
		 * and update all route entries associated with the interface,
		 * so that their MTU metric gets updated.
		 */
		if (ifp->if_mtu != oldmtu) {
			if_rtmtu_update(ifp);
			nd6_setmtu(ifp);
			/* Inform all transmit queues about the new MTU */
			IFCQ_LOCK(ifq);
			ifnet_update_sndq(ifq, CLASSQ_EV_LINK_MTU);
			IFCQ_UNLOCK(ifq);
		}
		break;
	}

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = proc_suser(p);
		if (error != 0) {
			break;
		}

		/* Don't allow group membership on non-multicast interfaces. */
		if ((ifp->if_flags & IFF_MULTICAST) == 0) {
			error = EOPNOTSUPP;
			break;
		}

		/* Don't let users screw up protocols' entries. */
		if (ifr->ifr_addr.sa_family != AF_UNSPEC &&
		    ifr->ifr_addr.sa_family != AF_LINK) {
			error = EINVAL;
			break;
		}
		if (ifr->ifr_addr.sa_len > sizeof(struct sockaddr)) {
			ifr->ifr_addr.sa_len = sizeof(struct sockaddr);
		}

		/*
		 * User is permitted to anonymously join a particular link
		 * multicast group via SIOCADDMULTI.  Subsequent join requested
		 * for the same record which has an outstanding refcnt from a
		 * past if_addmulti_anon() will not result in EADDRINUSE error
		 * (unlike other BSDs.)  Anonymously leaving a group is also
		 * allowed only as long as there is an outstanding refcnt held
		 * by a previous anonymous request, or else ENOENT (even if the
		 * link-layer multicast membership exists for a network-layer
		 * membership.)
		 */
		if (cmd == SIOCADDMULTI) {
			error = if_addmulti_anon(ifp, &ifr->ifr_addr, NULL);
			ev_msg.event_code = KEV_DL_ADDMULTI;
		} else {
			error = if_delmulti_anon(ifp, &ifr->ifr_addr);
			ev_msg.event_code = KEV_DL_DELMULTI;
		}
		if (error != 0) {
			break;
		}

		ev_msg.vendor_code    = KEV_VENDOR_APPLE;
		ev_msg.kev_class      = KEV_NETWORK_CLASS;
		ev_msg.kev_subclass   = KEV_DL_SUBCLASS;
		strlcpy(&ev_data.if_name[0], ifp->if_name, IFNAMSIZ);

		ev_data.if_family = ifp->if_family;
		ev_data.if_unit   = (u_int32_t) ifp->if_unit;
		ev_msg.dv[0].data_length = sizeof(struct net_event_data);
		ev_msg.dv[0].data_ptr    = &ev_data;
		ev_msg.dv[1].data_length = 0;
		dlil_post_complete_msg(ifp, &ev_msg);

		ifnet_touch_lastchange(ifp);
		break;

	case SIOCSIFMEDIA:
		error = proc_suser(p);
		if (error != 0) {
			break;
		}
		/*
		 * Silently ignore setting IFM_OTHER
		 */
		if (ifr->ifr_media == IFM_OTHER) {
			os_log_info(OS_LOG_DEFAULT,
			    "%s: %s SIOCSIFMEDIA ignore IFM_OTHER",
			    __func__, ifp->if_xname);
			error = 0;
			break;
		}
		error = ifnet_ioctl(ifp, SOCK_DOM(so), cmd, (caddr_t)ifr);
		if (error != 0) {
			break;
		}
		ifnet_touch_lastchange(ifp);
		break;

	case SIOCDIFPHYADDR:
	case SIOCSIFGENERIC:
	case SIOCSIFLLADDR:
	case SIOCSIFALTMTU:
	case SIOCSIFVLAN:
	case SIOCSIFBOND:
		error = proc_suser(p);
		if (error != 0) {
			break;
		}

		error = ifnet_ioctl(ifp, SOCK_DOM(so), cmd, (caddr_t)ifr);
		if (error != 0) {
			break;
		}

		ifnet_touch_lastchange(ifp);
		break;

	case SIOCGIFLLADDR: {
		struct sockaddr_dl *sdl = SDL(ifp->if_lladdr->ifa_addr);

		if (sdl->sdl_alen == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		/* If larger than 14-bytes we'll need another mechanism */
		if (sdl->sdl_alen > sizeof(ifr->ifr_addr.sa_data)) {
			error = EMSGSIZE;
			break;
		}
		/* Follow the same convention used by SIOCSIFLLADDR */
		SOCKADDR_ZERO(&ifr->ifr_addr, sizeof(ifr->ifr_addr));
		ifr->ifr_addr.sa_family = AF_LINK;
		ifr->ifr_addr.sa_len = sdl->sdl_alen;
		error = ifnet_guarded_lladdr_copy_bytes(ifp,
		    &ifr->ifr_addr.sa_data, sdl->sdl_alen);
		break;
	}

	case SIOCGIFTYPE:
		ifr->ifr_type.ift_type = ifp->if_type;
		ifr->ifr_type.ift_family = ifp->if_family;
		ifr->ifr_type.ift_subfamily = ifp->if_subfamily;
		break;

	case SIOCGIFFUNCTIONALTYPE:
		ifr->ifr_functional_type = if_functional_type(ifp, FALSE);
		break;

	case SIOCGIFPSRCADDR:
	case SIOCGIFPDSTADDR:
	case SIOCGIFGENERIC:
	case SIOCGIFDEVMTU:
	case SIOCGIFVLAN:
	case SIOCGIFBOND:
		error = ifnet_ioctl(ifp, SOCK_DOM(so), cmd, (caddr_t)ifr);
		break;

	case SIOCGIFWAKEFLAGS:
		ifnet_lock_shared(ifp);
		ifr->ifr_wake_flags = ifnet_get_wake_flags(ifp);
		ifnet_lock_done(ifp);
		break;

	case SIOCGIFGETRTREFCNT:
		ifnet_lock_shared(ifp);
		ifr->ifr_route_refcnt = ifp->if_route_refcnt;
		ifnet_lock_done(ifp);
		break;

	case SIOCSIFOPPORTUNISTIC:
	case SIOCGIFOPPORTUNISTIC:
		error = ifnet_getset_opportunistic(ifp, cmd, ifr, p);
		break;

	case SIOCGIFLINKQUALITYMETRIC:
		ifnet_lock_shared(ifp);
		if ((ifp->if_interface_state.valid_bitmask &
		    IF_INTERFACE_STATE_LQM_STATE_VALID)) {
			ifr->ifr_link_quality_metric =
			    ifp->if_interface_state.lqm_state;
		} else if (IF_FULLY_ATTACHED(ifp)) {
			ifr->ifr_link_quality_metric =
			    IFNET_LQM_THRESH_UNKNOWN;
		} else {
			ifr->ifr_link_quality_metric =
			    IFNET_LQM_THRESH_OFF;
		}
		ifnet_lock_done(ifp);
		break;

	case SIOCSIFLINKQUALITYMETRIC:
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		error = ifnet_set_link_quality(ifp, ifr->ifr_link_quality_metric);
		break;

	case SIOCSIFLOG:
	case SIOCGIFLOG:
		error = ifnet_getset_log(ifp, cmd, ifr, p);
		break;

	case SIOCGIFDELEGATE:
		ifnet_lock_shared(ifp);
		ifr->ifr_delegated = ((ifp->if_delegated.ifp != NULL) ?
		    ifp->if_delegated.ifp->if_index : 0);
		ifnet_lock_done(ifp);
		break;

	case SIOCGIFEXPENSIVE:
		ifnet_lock_shared(ifp);
		if (ifp->if_eflags & IFEF_EXPENSIVE) {
			ifr->ifr_expensive = 1;
		} else {
			ifr->ifr_expensive = 0;
		}
		ifnet_lock_done(ifp);
		break;

	case SIOCSIFEXPENSIVE:
	{
		struct ifnet *difp;

		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		if (ifr->ifr_expensive) {
			if_set_eflags(ifp, IFEF_EXPENSIVE);
		} else {
			if_clear_eflags(ifp, IFEF_EXPENSIVE);
		}
		ifnet_increment_generation(ifp);

		/*
		 * Update the expensive bit in the delegated interface
		 * structure.
		 */
		ifnet_head_lock_shared();
		TAILQ_FOREACH(difp, &ifnet_head, if_link) {
			ifnet_lock_exclusive(difp);
			if (difp->if_delegated.ifp == ifp) {
				difp->if_delegated.expensive =
				    ifp->if_eflags & IFEF_EXPENSIVE ? 1 : 0;
				ifnet_increment_generation(difp);
			}
			ifnet_lock_done(difp);
		}
		ifnet_head_done();
		necp_update_all_clients();
		break;
	}

	case SIOCGIFCONSTRAINED:
		if ((ifp->if_xflags & IFXF_CONSTRAINED) != 0) {
			ifr->ifr_constrained = 1;
		} else {
			ifr->ifr_constrained = 0;
		}
		break;

	case SIOCSIFCONSTRAINED:
	{
		struct ifnet *difp;

		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		if (ifr->ifr_constrained) {
			if_set_xflags(ifp, IFXF_CONSTRAINED);
		} else {
			if_clear_xflags(ifp, IFXF_CONSTRAINED);
		}
		ifnet_increment_generation(ifp);
		/*
		 * Update the constrained bit in the delegated interface
		 * structure.
		 */
		ifnet_head_lock_shared();
		TAILQ_FOREACH(difp, &ifnet_head, if_link) {
			ifnet_lock_exclusive(difp);
			if (difp->if_delegated.ifp == ifp) {
				difp->if_delegated.constrained =
				    ((ifp->if_xflags & IFXF_CONSTRAINED) != 0) ? 1 : 0;
				ifnet_increment_generation(difp);
			}
			ifnet_lock_done(difp);
		}
		ifnet_head_done();
		necp_update_all_clients();
		break;
	}

	case SIOCGIFULTRACONSTRAINED: {
		if ((ifp->if_xflags & IFXF_ULTRA_CONSTRAINED) != 0) {
			ifr->ifr_constrained = 1;
		} else {
			ifr->ifr_constrained = 0;
		}
		break;
	}

	case SIOCSIFULTRACONSTRAINED:
	{
		struct ifnet *difp;
		bool some_interface_is_ultra_constrained = false;

		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		if (ifr->ifr_constrained) {
			if_set_xflags(ifp, IFXF_ULTRA_CONSTRAINED);
		} else {
			if_clear_xflags(ifp, IFXF_ULTRA_CONSTRAINED);
		}
		ifnet_increment_generation(ifp);
		/*
		 * Update the ultra constrained bit in the delegated
		 * interface structure.
		 */
		ifnet_head_lock_shared();
		TAILQ_FOREACH(difp, &ifnet_head, if_link) {
			ifnet_lock_exclusive(difp);
			if (difp->if_xflags & IFXF_ULTRA_CONSTRAINED) {
				some_interface_is_ultra_constrained = true;
			}
			if (difp->if_delegated.ifp == ifp) {
				difp->if_delegated.ultra_constrained =
				    ((ifp->if_xflags & IFXF_ULTRA_CONSTRAINED) != 0) ? 1 : 0;
				ifnet_increment_generation(difp);
			}
			ifnet_lock_done(difp);
		}
		if_ultra_constrained_check_needed = some_interface_is_ultra_constrained;
		ifnet_head_done();
		necp_update_all_clients();
		break;
	}

	case SIOCSIFESTTHROUGHPUT:
	{
		bool changed = false;
		struct ifnet *difp;

		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		ifnet_lock_exclusive(ifp);
		changed = (ifp->if_estimated_up_bucket != ifr->ifr_estimated_throughput.up_bucket) ||
		    (ifp->if_estimated_down_bucket != ifr->ifr_estimated_throughput.down_bucket);
		ifp->if_estimated_up_bucket = ifr->ifr_estimated_throughput.up_bucket;
		ifp->if_estimated_down_bucket = ifr->ifr_estimated_throughput.down_bucket;
		if (changed) {
			ifnet_increment_generation(ifp);
		}
		ifnet_lock_done(ifp);
		os_log_info(OS_LOG_DEFAULT,
		    "SIOCSIFESTTHROUGHPUT %s%s up: %u, down: %u",
		    ifp->if_name, changed ? " changed" : "",
		    ifp->if_estimated_up_bucket,
		    ifp->if_estimated_down_bucket);
		if (changed) {
			/*
			 * Update the generation on delegated interfaces.
			 */
			ifnet_head_lock_shared();
			TAILQ_FOREACH(difp, &ifnet_head, if_link) {
				ifnet_lock_exclusive(difp);
				if (difp->if_delegated.ifp == ifp) {
					ifnet_increment_generation(difp);
				}
				ifnet_lock_done(difp);
			}
			ifnet_head_done();
			necp_update_all_clients();
		}
		break;
	}

	case SIOCSIFRADIODETAILS:
	{
		bool changed = false;
		struct ifnet *difp;

		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		ifnet_lock_exclusive(ifp);
		changed = ifp->if_radio_type != ifr->ifr_radio_details.technology ||
		    ifp->if_radio_channel != ifr->ifr_radio_details.channel;
		ifp->if_radio_type = ifr->ifr_radio_details.technology;
		ifp->if_radio_channel = ifr->ifr_radio_details.channel;
		ifnet_lock_done(ifp);
		os_log_info(OS_LOG_DEFAULT,
		    "SIOCSIFRADIODETAILS %s%s technology: %u, channel: %u",
		    ifp->if_name, changed ? " changed" : "",
		    ifr->ifr_radio_details.technology,
		    ifr->ifr_radio_details.channel);
		if (changed) {
			ifnet_increment_generation(ifp);
			/*
			 * Update the generation on delegated interfaces.
			 */
			ifnet_head_lock_shared();
			TAILQ_FOREACH(difp, &ifnet_head, if_link) {
				ifnet_lock_exclusive(difp);
				if (difp->if_delegated.ifp == ifp) {
					ifnet_increment_generation(difp);
				}
				ifnet_lock_done(difp);
			}
			ifnet_head_done();
			necp_update_all_clients();
		}
		break;
	}

	case SIOCGIF2KCL:
		ifnet_lock_shared(ifp);
		if (ifp->if_eflags & IFEF_2KCL) {
			ifr->ifr_2kcl = 1;
		} else {
			ifr->ifr_2kcl = 0;
		}
		ifnet_lock_done(ifp);
		break;

	case SIOCSIF2KCL:
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		if (ifr->ifr_2kcl) {
			if_set_eflags(ifp, IFEF_2KCL);
		} else {
			if_clear_eflags(ifp, IFEF_2KCL);
		}
		break;
	case SIOCGSTARTDELAY:
		ifnet_lock_shared(ifp);
		if (ifp->if_eflags & IFEF_ENQUEUE_MULTI) {
			ifr->ifr_start_delay_qlen =
			    ifp->if_start_delay_qlen;
			ifr->ifr_start_delay_timeout =
			    ifp->if_start_delay_timeout;
		} else {
			ifr->ifr_start_delay_qlen = 0;
			ifr->ifr_start_delay_timeout = 0;
		}
		ifnet_lock_done(ifp);
		break;
	case SIOCSIFDSTADDR:
	case SIOCSIFADDR:
	case SIOCSIFBRDADDR:
	case SIOCSIFNETMASK:
	case OSIOCGIFADDR:
	case OSIOCGIFDSTADDR:
	case OSIOCGIFBRDADDR:
	case OSIOCGIFNETMASK:
	case SIOCSIFKPI:
		VERIFY(so->so_proto != NULL);

		if (cmd == SIOCSIFDSTADDR || cmd == SIOCSIFADDR ||
		    cmd == SIOCSIFBRDADDR || cmd == SIOCSIFNETMASK) {
#if BYTE_ORDER != BIG_ENDIAN
			if (ifr->ifr_addr.sa_family == 0 &&
			    ifr->ifr_addr.sa_len < 16) {
				ifr->ifr_addr.sa_family = ifr->ifr_addr.sa_len;
				ifr->ifr_addr.sa_len = 16;
			}
#else
			if (ifr->ifr_addr.sa_len == 0) {
				ifr->ifr_addr.sa_len = 16;
			}
#endif
		} else if (cmd == OSIOCGIFADDR) {
			cmd = SIOCGIFADDR;      /* struct ifreq */
		} else if (cmd == OSIOCGIFDSTADDR) {
			cmd = SIOCGIFDSTADDR;   /* struct ifreq */
		} else if (cmd == OSIOCGIFBRDADDR) {
			cmd = SIOCGIFBRDADDR;   /* struct ifreq */
		} else if (cmd == OSIOCGIFNETMASK) {
			cmd = SIOCGIFNETMASK;   /* struct ifreq */
		}

		socket_lock(so, 1);
		error = ((*so->so_proto->pr_usrreqs->pru_control)(so, cmd,
		    (caddr_t)(struct ifreq *__indexable)ifr, ifp, p));
		socket_unlock(so, 1);

		switch (ocmd) {
		case OSIOCGIFADDR:
		case OSIOCGIFDSTADDR:
		case OSIOCGIFBRDADDR:
		case OSIOCGIFNETMASK:
			SOCKADDR_COPY(&ifr->ifr_addr.sa_family, &ifr->ifr_addr,
			    sizeof(u_short));
		}

		if (cmd == SIOCSIFKPI) {
			int temperr = proc_suser(p);
			if (temperr != 0) {
				error = temperr;
			}
		}
		// Don't allow to call SIOCSIFADDR and SIOCSIFDSTADDR
		// with ifreq as the code expects ifaddr
		if ((error == EOPNOTSUPP || error == ENOTSUP) &&
		    !(cmd == SIOCSIFADDR || cmd == SIOCSIFDSTADDR)) {
			error = ifnet_ioctl(ifp, SOCK_DOM(so), cmd,
			    (caddr_t)ifr);
		}
		break;

	case SIOCGIFINTERFACESTATE:
		if_get_state(ifp, &ifr->ifr_interface_state);
		break;

	case SIOCSIFINTERFACESTATE:
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}

		error = if_state_update(ifp, &ifr->ifr_interface_state);

		break;
	case SIOCSIFPROBECONNECTIVITY:
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		error = if_probe_connectivity(ifp,
		    ifr->ifr_probe_connectivity);
		break;
	case SIOCGIFPROBECONNECTIVITY:
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		if (ifp->if_eflags & IFEF_PROBE_CONNECTIVITY) {
			ifr->ifr_probe_connectivity = 1;
		} else {
			ifr->ifr_probe_connectivity = 0;
		}
		break;
	case SIOCGECNMODE:
		if ((ifp->if_eflags & (IFEF_ECN_ENABLE | IFEF_ECN_DISABLE)) ==
		    IFEF_ECN_ENABLE) {
			ifr->ifr_ecn_mode = IFRTYPE_ECN_ENABLE;
		} else if ((ifp->if_eflags & (IFEF_ECN_ENABLE | IFEF_ECN_DISABLE)) ==
		    IFEF_ECN_DISABLE) {
			ifr->ifr_ecn_mode = IFRTYPE_ECN_DISABLE;
		} else {
			ifr->ifr_ecn_mode = IFRTYPE_ECN_DEFAULT;
		}
		break;
	case SIOCSECNMODE:
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		if (ifr->ifr_ecn_mode == IFRTYPE_ECN_DEFAULT) {
			if_clear_eflags(ifp, IFEF_ECN_ENABLE | IFEF_ECN_DISABLE);
		} else if (ifr->ifr_ecn_mode == IFRTYPE_ECN_ENABLE) {
			if_set_eflags(ifp, IFEF_ECN_ENABLE);
			if_clear_eflags(ifp, IFEF_ECN_DISABLE);
		} else if (ifr->ifr_ecn_mode == IFRTYPE_ECN_DISABLE) {
			if_set_eflags(ifp, IFEF_ECN_DISABLE);
			if_clear_eflags(ifp, IFEF_ECN_ENABLE);
		} else {
			error = EINVAL;
		}
		break;

	case SIOCSIFTIMESTAMPENABLE:
	case SIOCSIFTIMESTAMPDISABLE:
		error = proc_suser(p);
		if (error != 0) {
			break;
		}

		if ((cmd == SIOCSIFTIMESTAMPENABLE &&
		    (ifp->if_xflags & IFXF_TIMESTAMP_ENABLED) != 0) ||
		    (cmd == SIOCSIFTIMESTAMPDISABLE &&
		    (ifp->if_xflags & IFXF_TIMESTAMP_ENABLED) == 0)) {
			break;
		}
		if (cmd == SIOCSIFTIMESTAMPENABLE) {
			if_set_xflags(ifp, IFXF_TIMESTAMP_ENABLED);
		} else {
			if_clear_xflags(ifp, IFXF_TIMESTAMP_ENABLED);
		}
		/*
		 * Pass the setting to the interface if it supports either
		 * software or hardware time stamping
		 */
		if (ifp->if_capabilities & (IFCAP_HW_TIMESTAMP |
		    IFCAP_SW_TIMESTAMP)) {
			error = ifnet_ioctl(ifp, SOCK_DOM(so), cmd,
			    (caddr_t)ifr);
		}
		break;
	case SIOCGIFTIMESTAMPENABLED: {
		if ((ifp->if_xflags & IFXF_TIMESTAMP_ENABLED) != 0) {
			ifr->ifr_intval = 1;
		} else {
			ifr->ifr_intval = 0;
		}
		break;
	}
	case SIOCSQOSMARKINGMODE:
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		error = if_set_qosmarking_mode(ifp, ifr->ifr_qosmarking_mode);
		break;

	case SIOCGQOSMARKINGMODE:
		ifr->ifr_qosmarking_mode = ifp->if_qosmarking_mode;
		break;

	case SIOCSQOSMARKINGENABLED:
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		if (ifr->ifr_qosmarking_enabled != 0) {
			if_set_eflags(ifp, IFEF_QOSMARKING_ENABLED);
		} else {
			if_clear_eflags(ifp, IFEF_QOSMARKING_ENABLED);
		}
		break;

	case SIOCGQOSMARKINGENABLED:
		ifr->ifr_qosmarking_enabled =
		    ((ifp->if_eflags & IFEF_QOSMARKING_ENABLED) != 0) ? 1 : 0;
		break;

	case SIOCSIFDISABLEOUTPUT:
#if (DEBUG || DEVELOPMENT)
		if (ifr->ifr_disable_output == 1) {
			error = ifnet_disable_output(ifp);
		} else if (ifr->ifr_disable_output == 0) {
			error = ifnet_enable_output(ifp);
		} else {
			error = EINVAL;
		}
#else
		error = EINVAL;
#endif /* (DEBUG || DEVELOPMENT) */
		break;

	case SIOCSIFSUBFAMILY:
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
#if (DEBUG || DEVELOPMENT)
		if (management_control_unrestricted) {
			uint32_t subfamily = ifr->ifr_type.ift_subfamily;

			if (subfamily == ifp->if_subfamily) {
				break;
			} else if (subfamily == IFRTYPE_SUBFAMILY_MANAGEMENT && ifp->if_subfamily == 0) {
				ifp->if_subfamily = IFNET_SUBFAMILY_MANAGEMENT;
				ifnet_set_management(ifp, true);
				break;
			} else if (subfamily == 0 && ifp->if_subfamily == IFNET_SUBFAMILY_MANAGEMENT) {
				ifnet_set_management(ifp, false);
				break;
			}
		}
#endif /* (DEBUG || DEVELOPMENT) */
		error = ifnet_ioctl(ifp, SOCK_DOM(so), cmd, (caddr_t)ifr);
		break;

	case SIOCSIFPEEREGRESSFUNCTIONALTYPE:
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		error = ifnet_ioctl(ifp, SOCK_DOM(so), cmd, (caddr_t)ifr);
		break;

	case SIOCSIFMANAGEMENT: {
		if (management_control_unrestricted == false &&
		    !IOCurrentTaskHasEntitlement(MANAGEMENT_CONTROL_ENTITLEMENT)) {
			os_log(OS_LOG_DEFAULT, "ifioctl_req: cmd SIOCSIFMANAGEMENT on %s not allowed for %s:%u\n",
			    ifp->if_xname, proc_name_address(p), proc_pid(p));
			return EPERM;
		}
		if (ifr->ifr_intval != 0) {
			ifnet_set_management(ifp, true);
		} else {
			if (ifp->if_subfamily == IFNET_SUBFAMILY_MANAGEMENT) {
				os_log(OS_LOG_DEFAULT, "ifioctl_req: cmd SIOCSIFMANAGEMENT 0 not allowed on %s with subfamily management",
				    ifp->if_xname);
				return EPERM;
			}
			ifnet_set_management(ifp, false);
		}
		break;
	}

	case SIOCSATTACHPROTONULL: {
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		if (ifr->ifr_intval != 0) {
			struct ifnet_attach_proto_param reg = {};

			reg.input = null_proto_input;

			error = ifnet_attach_protocol(ifp, PF_NULL, &reg);
			if (error != 0) {
				os_log(OS_LOG_DEFAULT,
				    "ifioctl_req: SIOCSATTACHPROTONULL ifnet_attach_protocol(%s) failed, %d",
				    ifp->if_xname, error);
			}
		} else {
			error = ifnet_detach_protocol(ifp, PF_NULL);
			if (error != 0) {
				os_log(OS_LOG_DEFAULT,
				    "ifioctl_req: SIOCSATTACHPROTONULL ifnet_detach_protocol(%s) failed, %d",
				    ifp->if_xname, error);
			}
		}
		break;
	}

	case SIOCSIFLOWINTERNET:
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}

		if (ifr->ifr_low_internet & IFRTYPE_LOW_INTERNET_ENABLE_UL) {
			if_set_xflags(ifp, IFXF_LOW_INTERNET_UL);
		} else {
			if_clear_xflags(ifp, IFXF_LOW_INTERNET_UL);
		}
		if (ifr->ifr_low_internet & IFRTYPE_LOW_INTERNET_ENABLE_DL) {
			if_set_xflags(ifp, IFXF_LOW_INTERNET_DL);
		} else {
			if_clear_xflags(ifp, IFXF_LOW_INTERNET_DL);
		}
		break;
	case SIOCGIFLOWINTERNET:
		ifnet_lock_shared(ifp);
		ifr->ifr_low_internet = 0;
		if ((ifp->if_xflags & IFXF_LOW_INTERNET_UL) != 0) {
			ifr->ifr_low_internet |=
			    IFRTYPE_LOW_INTERNET_ENABLE_UL;
		}
		if ((ifp->if_xflags & IFXF_LOW_INTERNET_DL) != 0) {
			ifr->ifr_low_internet |=
			    IFRTYPE_LOW_INTERNET_ENABLE_DL;
		}
		ifnet_lock_done(ifp);
		break;
	case SIOCGIFLOWPOWER:
		ifr->ifr_low_power_mode =
		    ((ifp->if_xflags & IFXF_LOW_POWER) != 0);
		break;
	case SIOCSIFLOWPOWER:
#if (DEVELOPMENT || DEBUG)
		error = if_set_low_power(ifp, (ifr->ifr_low_power_mode != 0));
#else /* DEVELOPMENT || DEBUG */
		error = EOPNOTSUPP;
#endif /* DEVELOPMENT || DEBUG */
		break;

	case SIOCGIFMPKLOG:
		ifr->ifr_mpk_log = ((ifp->if_xflags & IFXF_MPK_LOG) != 0);
		break;
	case SIOCSIFMPKLOG:
		if (ifr->ifr_mpk_log) {
			if_set_xflags(ifp, IFXF_MPK_LOG);
		} else {
			if_clear_xflags(ifp, IFXF_MPK_LOG);
		}
		break;
	case SIOCGIFNOACKPRIO:
		if ((ifp->if_eflags & IFEF_NOACKPRI) != 0) {
			ifr->ifr_noack_prio = 1;
		} else {
			ifr->ifr_noack_prio = 0;
		}
		break;

	case SIOCSIFNOACKPRIO:
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		if (ifr->ifr_noack_prio) {
			if_set_eflags(ifp, IFEF_NOACKPRI);
		} else {
			if_clear_eflags(ifp, IFEF_NOACKPRI);
		}
		break;

	case SIOCSIFMARKWAKEPKT:
#if (DEVELOPMENT || DEBUG)
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		if (net_wake_pkt_debug) {
			os_log(OS_LOG_DEFAULT,
			    "SIOCSIFMARKWAKEPKT %s", ifp->if_xname);
		}
		if (ifr->ifr_intval != 0) {
			ifp->if_xflags |= IFXF_MARK_WAKE_PKT;
		} else {
			ifp->if_xflags &= ~IFXF_MARK_WAKE_PKT;
		}
#else /* DEVELOPMENT || DEBUG */
		error = EOPNOTSUPP;
#endif /* DEVELOPMENT || DEBUG */
		break;

	case SIOCSIFNOTRAFFICSHAPING:
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		os_log_info(OS_LOG_DEFAULT, "SIOCSIFNOTRAFFICSHAPING %s %d",
		    ifp->if_xname, ifr->ifr_intval);
		if (ifr->ifr_intval != 0) {
			ifp->if_xflags |= IFXF_NO_TRAFFIC_SHAPING;
		} else {
			ifp->if_xflags &= ~IFXF_NO_TRAFFIC_SHAPING;
		}
		break;

	case SIOCGIFNOTRAFFICSHAPING:
		if ((ifp->if_xflags & IFXF_NO_TRAFFIC_SHAPING) != 0) {
			ifr->ifr_intval = 1;
		} else {
			ifr->ifr_intval = 0;
		}
		break;

	case SIOCGIFGENERATIONID:
		ifr->ifr_creation_generation_id = ifp->if_creation_generation_id;
		break;

	case SIOCSIFDIRECTLINK:
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		if (ifr->ifr_is_directlink) {
			if_set_eflags(ifp, IFEF_DIRECTLINK);
		} else {
			if_clear_eflags(ifp, IFEF_DIRECTLINK);
		}
		break;

	case SIOCGIFDIRECTLINK:
		ifnet_lock_shared(ifp);
		ifr->ifr_is_directlink = (ifp->if_eflags & IFEF_DIRECTLINK) ? true : false;
		ifnet_lock_done(ifp);
		break;

	case SIOCSIFISVPN:
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		if (ifr->ifr_is_vpn) {
			if_set_xflags(ifp, IFXF_IS_VPN);
		} else {
			if_clear_xflags(ifp, IFXF_IS_VPN);
		}
		break;

	case SIOCSIFDELAYWAKEPKTEVENT:
		if (!IOCurrentTaskHasEntitlement(WAKE_PKT_EVENT_CONTROL_ENTITLEMENT)) {
			return EPERM;
		}
		if (ifr->ifr_delay_wake_pkt_event == 0) {
			ifp->if_xflags &= ~IFXF_DELAYWAKEPKTEVENT;
		} else {
			ifp->if_xflags |= IFXF_DELAYWAKEPKTEVENT;
		}
		break;
	case SIOCGIFDELAYWAKEPKTEVENT:
		ifr->ifr_delay_wake_pkt_event =
		    ((ifp->if_xflags & IFXF_DELAYWAKEPKTEVENT) != 0) ? 1 : 0;
		break;

	case SIOCSIFDISABLEINPUT:
#if (DEBUG || DEVELOPMENT)
		if ((error = priv_check_cred(kauth_cred_get(),
		    PRIV_NET_INTERFACE_CONTROL, 0)) != 0) {
			return error;
		}
		if (ifr->ifr_intval != 0) {
			if ((ifp->if_xflags & IFXF_DISABLE_INPUT) != 0) {
				error = EALREADY;
				break;
			}
			if_set_xflags(ifp, IFXF_DISABLE_INPUT);
		} else {
			if_clear_xflags(ifp, IFXF_DISABLE_INPUT);
		}
		os_log(OS_LOG_DEFAULT, "SIOCSIFDISABLEINPUT %s disable input %d",
		    ifp->if_xname, ifr->ifr_intval);
#else
		error = EINVAL;
#endif /* (DEBUG || DEVELOPMENT) */
		break;

	case SIOCGIFDISABLEINPUT:
		ifr->ifr_intval =
		    (ifp->if_xflags & IFXF_DISABLE_INPUT) != 0 ? 1 : 0;
		break;

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return error;
}

int
ifioctllocked(struct socket *so, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data, struct proc *p)
{
	int error;

	socket_unlock(so, 0);
	error = ifioctl(so, cmd, data, p);
	socket_lock(so, 0);
	return error;
}

/*
 * Set/clear promiscuous mode on interface ifp based on the truth value
 * of pswitch.  The calls are reference counted so that only the first
 * "on" request actually has an effect, as does the final "off" request.
 * Results are undefined if the "off" and "on" requests are not matched.
 */
errno_t
ifnet_set_promiscuous(
	ifnet_t ifp,
	int pswitch)
{
	int error = 0;
	int oldflags = 0;
	int newflags = 0;

	ifnet_lock_exclusive(ifp);
	oldflags = ifp->if_flags;
	ifp->if_pcount += pswitch ? 1 : -1;

	if (ifp->if_pcount > 0) {
		ifp->if_flags |= IFF_PROMISC;
	} else {
		ifp->if_flags &= ~IFF_PROMISC;
	}

	newflags = ifp->if_flags;
	ifnet_lock_done(ifp);

	if (newflags != oldflags && (newflags & IFF_UP) != 0) {
		error = ifnet_ioctl(ifp, 0, SIOCSIFFLAGS, NULL);
		if (error == 0) {
			rt_ifmsg(ifp);
		} else {
			ifnet_lock_exclusive(ifp);
			// revert the flags
			ifp->if_pcount -= pswitch ? 1 : -1;
			if (ifp->if_pcount > 0) {
				ifp->if_flags |= IFF_PROMISC;
			} else {
				ifp->if_flags &= ~IFF_PROMISC;
			}
			ifnet_lock_done(ifp);
		}
	}

	if (newflags != oldflags) {
		log(LOG_INFO, "%s: promiscuous mode %s %s (%d)\n",
		    if_name(ifp),
		    (newflags & IFF_PROMISC) != 0 ? "enable" : "disable",
		    error != 0 ? "failed" : "succeeded", error);
	}
	return error;
}

/*
 * Return interface configuration
 * of system.  List may be used
 * in later ioctl's (above) to get
 * other information.
 */
/*ARGSUSED*/
static int
ifconf(u_long cmd, user_addr_t ifrp, int *ret_space)
{
	struct ifnet *ifp = NULL;
	struct ifaddr *ifa;
	struct ifreq ifr;
	int error = 0;
	size_t space;
	net_thread_marks_t marks;

	marks = net_thread_marks_push(NET_THREAD_CKREQ_LLADDR);

	/*
	 * Zero the ifr buffer to make sure we don't
	 * disclose the contents of the stack.
	 */
	bzero(&ifr, sizeof(struct ifreq));

	space = *ret_space;
	ifnet_head_lock_shared();
	for (ifp = ifnet_head.tqh_first; space > sizeof(ifr) &&
	    ifp; ifp = ifp->if_link.tqe_next) {
		char workbuf[64];
		size_t ifnlen, addrs;

		ifnlen = snprintf(workbuf, sizeof(workbuf),
		    "%s", if_name(ifp));
		if (ifnlen + 1 > sizeof(ifr.ifr_name)) {
			error = ENAMETOOLONG;
			break;
		} else {
			strbufcpy(ifr.ifr_name, IFNAMSIZ, workbuf, sizeof(workbuf));
		}

		ifnet_lock_shared(ifp);

		addrs = 0;
		ifa = ifp->if_addrhead.tqh_first;
		for (; space > sizeof(ifr) && ifa;
		    ifa = ifa->ifa_link.tqe_next) {
			struct sockaddr *__single sa;
			union {
				struct sockaddr sa;
				struct sockaddr_dl sdl;
				uint8_t buf[SOCK_MAXADDRLEN + 1];
			} u;

			/*
			 * Make sure to accomodate the largest possible
			 * size of SA(if_lladdr)->sa_len.
			 */
			_CASSERT(sizeof(u) == (SOCK_MAXADDRLEN + 1));

			bzero(u.buf, sizeof(u.buf));

			IFA_LOCK(ifa);
			sa = ifa->ifa_addr;
			addrs++;

			if (ifa == ifp->if_lladdr) {
				VERIFY(sa->sa_family == AF_LINK);
				SOCKADDR_COPY(sa, &u.sa, sa->sa_len);
				IFA_UNLOCK(ifa);
				ifnet_guarded_lladdr_copy_bytes(ifp,
				    LLADDR(&u.sdl), u.sdl.sdl_alen);
				IFA_LOCK(ifa);
				sa = &u.sa;
			}

			if (cmd == OSIOCGIFCONF32 || cmd == OSIOCGIFCONF64) {
				struct osockaddr *__single osa =
				    (struct osockaddr *)(void *)&ifr.ifr_addr;
				ifr.ifr_addr = *sa;
				osa->sa_family = sa->sa_family;
				error = copyout((caddr_t)&ifr, ifrp,
				    sizeof(ifr));
				ifrp += sizeof(struct ifreq);
			} else if (sa->sa_len <= sizeof(*sa)) {
				ifr.ifr_addr = *sa;
				error = copyout((caddr_t)&ifr, ifrp,
				    sizeof(ifr));
				ifrp += sizeof(struct ifreq);
			} else {
				if (space <
				    sizeof(ifr) + sa->sa_len - sizeof(*sa)) {
					IFA_UNLOCK(ifa);
					break;
				}
				space -= sa->sa_len - sizeof(*sa);
				error = copyout((caddr_t)&ifr, ifrp,
				    sizeof(ifr.ifr_name));
				if (error == 0) {
					error = copyout(__SA_UTILS_CONV_TO_BYTES(sa), (ifrp +
					    offsetof(struct ifreq, ifr_addr)),
					    sa->sa_len);
				}
				ifrp += (sa->sa_len + offsetof(struct ifreq,
				    ifr_addr));
			}
			IFA_UNLOCK(ifa);
			if (error) {
				break;
			}
			space -= sizeof(ifr);
		}
		ifnet_lock_done(ifp);

		if (error) {
			break;
		}
		if (!addrs) {
			bzero((caddr_t)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			error = copyout((caddr_t)&ifr, ifrp, sizeof(ifr));
			if (error) {
				break;
			}
			space -= sizeof(ifr);
			ifrp += sizeof(struct ifreq);
		}
	}
	ifnet_head_done();
	*ret_space -= space;
	net_thread_marks_pop(marks);
	return error;
}

static bool
set_allmulti(struct ifnet * ifp, bool enable)
{
	bool    changed = false;

	ifnet_lock_exclusive(ifp);
	if (enable) {
		if (ifp->if_amcount++ == 0) {
			ifp->if_flags |= IFF_ALLMULTI;
			changed = true;
		}
	} else {
		if (ifp->if_amcount > 1) {
			ifp->if_amcount--;
		} else {
			ifp->if_amcount = 0;
			ifp->if_flags &= ~IFF_ALLMULTI;
			changed = true;
		}
	}
	ifnet_lock_done(ifp);
	return changed;
}

/*
 * Like ifnet_set_promiscuous(), but for all-multicast-reception mode.
 */
int
if_allmulti(struct ifnet *ifp, int onswitch)
{
	bool    enable = onswitch != 0;
	int     error = 0;

	if (set_allmulti(ifp, enable)) {
		/* state change, tell the driver */
		error = ifnet_ioctl(ifp, 0, SIOCSIFFLAGS, NULL);
		log(LOG_INFO, "%s: %s allmulti %s (%d)\n",
		    if_name(ifp),
		    enable ? "enable" : "disable",
		    error != 0 ? "failed" : "succeeded", error);
		if (error == 0) {
			rt_ifmsg(ifp);
		} else {
			/* restore the reference count, flags */
			(void)set_allmulti(ifp, !enable);
		}
	}
	return error;
}

static struct ifmultiaddr *
ifma_alloc(zalloc_flags_t how)
{
	struct ifmultiaddr *__single ifma;

	ifma = zalloc_flags(ifma_zone, how | Z_ZERO);

	if (ifma != NULL) {
		lck_mtx_init(&ifma->ifma_lock, &ifa_mtx_grp, &ifa_mtx_attr);
		ifma->ifma_debug |= IFD_ALLOC;
		if (ifma_debug != 0) {
			ifma->ifma_debug |= IFD_DEBUG;
			ifma->ifma_trace = ifma_trace;
		}
	}
	return ifma;
}

static void
ifma_free(struct ifmultiaddr *ifma)
{
	IFMA_LOCK(ifma);

	if (ifma->ifma_protospec != NULL) {
		panic("%s: Protospec not NULL for ifma=%p", __func__, ifma);
		/* NOTREACHED */
	} else if ((ifma->ifma_flags & IFMAF_ANONYMOUS) ||
	    ifma->ifma_anoncnt != 0) {
		panic("%s: Freeing ifma=%p with outstanding anon req",
		    __func__, ifma);
		/* NOTREACHED */
	} else if (ifma->ifma_debug & IFD_ATTACHED) {
		panic("%s: ifma=%p attached to ifma_ifp=%p is being freed",
		    __func__, ifma, ifma->ifma_ifp);
		/* NOTREACHED */
	} else if (!(ifma->ifma_debug & IFD_ALLOC)) {
		panic("%s: ifma %p cannot be freed", __func__, ifma);
		/* NOTREACHED */
	} else if (ifma->ifma_refcount != 0) {
		panic("%s: non-zero refcount ifma=%p", __func__, ifma);
		/* NOTREACHED */
	} else if (ifma->ifma_reqcnt != 0) {
		panic("%s: non-zero reqcnt ifma=%p", __func__, ifma);
		/* NOTREACHED */
	} else if (ifma->ifma_ifp != NULL) {
		panic("%s: non-NULL ifma_ifp=%p for ifma=%p", __func__,
		    ifma->ifma_ifp, ifma);
		/* NOTREACHED */
	} else if (ifma->ifma_ll != NULL) {
		panic("%s: non-NULL ifma_ll=%p for ifma=%p", __func__,
		    ifma->ifma_ll, ifma);
		/* NOTREACHED */
	}
	ifma->ifma_debug &= ~IFD_ALLOC;
	if ((ifma->ifma_debug & (IFD_DEBUG | IFD_TRASHED)) ==
	    (IFD_DEBUG | IFD_TRASHED)) {
		lck_mtx_lock(&ifma_trash_lock);
		TAILQ_REMOVE(&ifma_trash_head, (struct ifmultiaddr_dbg *)ifma,
		    ifma_trash_link);
		lck_mtx_unlock(&ifma_trash_lock);
		ifma->ifma_debug &= ~IFD_TRASHED;
	}
	IFMA_UNLOCK(ifma);

	if (ifma->ifma_addr != NULL) {
		kfree_data(ifma->ifma_addr, ifma->ifma_addr->sa_len);
		ifma->ifma_addr = NULL;
	}
	lck_mtx_destroy(&ifma->ifma_lock, &ifa_mtx_grp);
	zfree(ifma_zone, ifma);
}

static void
ifma_trace(struct ifmultiaddr *ifma, int refhold)
{
	struct ifmultiaddr_dbg *ifma_dbg = (struct ifmultiaddr_dbg *)ifma;
	ctrace_t *tr;
	u_int32_t idx;
	u_int16_t *cnt;

	if (!(ifma->ifma_debug & IFD_DEBUG)) {
		panic("%s: ifma %p has no debug structure", __func__, ifma);
		/* NOTREACHED */
	}
	if (refhold) {
		cnt = &ifma_dbg->ifma_refhold_cnt;
		tr = ifma_dbg->ifma_refhold;
	} else {
		cnt = &ifma_dbg->ifma_refrele_cnt;
		tr = ifma_dbg->ifma_refrele;
	}

	idx = os_atomic_inc_orig(cnt, relaxed) % IFMA_TRACE_HIST_SIZE;
	ctrace_record(&tr[idx]);
}

void
ifma_addref(struct ifmultiaddr *ifma, int locked)
{
	if (!locked) {
		IFMA_LOCK(ifma);
	} else {
		IFMA_LOCK_ASSERT_HELD(ifma);
	}

	if (++ifma->ifma_refcount == 0) {
		panic("%s: ifma=%p wraparound refcnt", __func__, ifma);
		/* NOTREACHED */
	} else if (ifma->ifma_trace != NULL) {
		(*ifma->ifma_trace)(ifma, TRUE);
	}
	if (!locked) {
		IFMA_UNLOCK(ifma);
	}
}

void
ifma_remref(struct ifmultiaddr *ifma)
{
	struct ifmultiaddr *__single ll;

	IFMA_LOCK(ifma);

	if (ifma->ifma_refcount == 0) {
		panic("%s: ifma=%p negative refcnt", __func__, ifma);
		/* NOTREACHED */
	} else if (ifma->ifma_trace != NULL) {
		(*ifma->ifma_trace)(ifma, FALSE);
	}

	--ifma->ifma_refcount;
	if (ifma->ifma_refcount > 0) {
		IFMA_UNLOCK(ifma);
		return;
	}

	ll = ifma->ifma_ll;
	ifma->ifma_ifp = NULL;
	ifma->ifma_ll = NULL;
	IFMA_UNLOCK(ifma);
	ifma_free(ifma);        /* deallocate it */

	if (ll != NULL) {
		IFMA_REMREF(ll);
	}
}

static void
if_attach_ifma(struct ifnet *ifp, struct ifmultiaddr *ifma, int anon)
{
	ifnet_lock_assert(ifp, IFNET_LCK_ASSERT_EXCLUSIVE);
	IFMA_LOCK_ASSERT_HELD(ifma);

	if (ifma->ifma_ifp != ifp) {
		panic("%s: Mismatch ifma_ifp=%p != ifp=%p", __func__,
		    ifma->ifma_ifp, ifp);
		/* NOTREACHED */
	} else if (ifma->ifma_debug & IFD_ATTACHED) {
		panic("%s: Attempt to attach an already attached ifma=%p",
		    __func__, ifma);
		/* NOTREACHED */
	} else if (anon && (ifma->ifma_flags & IFMAF_ANONYMOUS)) {
		panic("%s: ifma=%p unexpected IFMAF_ANONYMOUS", __func__, ifma);
		/* NOTREACHED */
	} else if (ifma->ifma_debug & IFD_TRASHED) {
		panic("%s: Attempt to reattach a detached ifma=%p",
		    __func__, ifma);
		/* NOTREACHED */
	}

	ifma->ifma_reqcnt++;
	VERIFY(ifma->ifma_reqcnt == 1);
	IFMA_ADDREF_LOCKED(ifma);
	ifma->ifma_debug |= IFD_ATTACHED;
	if (anon) {
		ifma->ifma_anoncnt++;
		VERIFY(ifma->ifma_anoncnt == 1);
		ifma->ifma_flags |= IFMAF_ANONYMOUS;
	}

	LIST_INSERT_HEAD(&ifp->if_multiaddrs, ifma, ifma_link);
}

static int
if_detach_ifma(struct ifnet *ifp, struct ifmultiaddr *ifma, int anon)
{
	ifnet_lock_assert(ifp, IFNET_LCK_ASSERT_EXCLUSIVE);
	IFMA_LOCK_ASSERT_HELD(ifma);

	if (ifma->ifma_reqcnt == 0) {
		panic("%s: ifma=%p negative reqcnt", __func__, ifma);
		/* NOTREACHED */
	} else if (anon && !(ifma->ifma_flags & IFMAF_ANONYMOUS)) {
		panic("%s: ifma=%p missing IFMAF_ANONYMOUS", __func__, ifma);
		/* NOTREACHED */
	} else if (anon && ifma->ifma_anoncnt == 0) {
		panic("%s: ifma=%p negative anonreqcnt", __func__, ifma);
		/* NOTREACHED */
	} else if (ifma->ifma_ifp != ifp) {
		panic("%s: Mismatch ifma_ifp=%p, ifp=%p", __func__,
		    ifma->ifma_ifp, ifp);
		/* NOTREACHED */
	}

	if (anon) {
		--ifma->ifma_anoncnt;
		if (ifma->ifma_anoncnt > 0) {
			return 0;
		}
		ifma->ifma_flags &= ~IFMAF_ANONYMOUS;
	}

	--ifma->ifma_reqcnt;
	if (ifma->ifma_reqcnt > 0) {
		return 0;
	}

	if (ifma->ifma_protospec != NULL) {
		panic("%s: Protospec not NULL for ifma=%p", __func__, ifma);
		/* NOTREACHED */
	} else if ((ifma->ifma_flags & IFMAF_ANONYMOUS) ||
	    ifma->ifma_anoncnt != 0) {
		panic("%s: Detaching ifma=%p with outstanding anon req",
		    __func__, ifma);
		/* NOTREACHED */
	} else if (!(ifma->ifma_debug & IFD_ATTACHED)) {
		panic("%s: Attempt to detach an unattached address ifma=%p",
		    __func__, ifma);
		/* NOTREACHED */
	} else if (ifma->ifma_debug & IFD_TRASHED) {
		panic("%s: ifma %p is already in trash list", __func__, ifma);
		/* NOTREACHED */
	}

	/*
	 * NOTE: Caller calls IFMA_REMREF
	 */
	ifma->ifma_debug &= ~IFD_ATTACHED;
	LIST_REMOVE(ifma, ifma_link);
	if (LIST_EMPTY(&ifp->if_multiaddrs)) {
		ifp->if_updatemcasts = 0;
	}

	if (ifma->ifma_debug & IFD_DEBUG) {
		/* Become a regular mutex, just in case */
		IFMA_CONVERT_LOCK(ifma);
		lck_mtx_lock(&ifma_trash_lock);
		TAILQ_INSERT_TAIL(&ifma_trash_head,
		    (struct ifmultiaddr_dbg *)ifma, ifma_trash_link);
		lck_mtx_unlock(&ifma_trash_lock);
		ifma->ifma_debug |= IFD_TRASHED;
	}

	return 1;
}

/*
 * Find an ifmultiaddr that matches a socket address on an interface.
 *
 * Caller is responsible for holding the ifnet_lock while calling
 * this function.
 */
static int
if_addmulti_doesexist(struct ifnet *ifp, const struct sockaddr *sa,
    struct ifmultiaddr **retifma, int anon)
{
	struct ifmultiaddr *__single ifma;

	for (ifma = LIST_FIRST(&ifp->if_multiaddrs); ifma != NULL;
	    ifma = LIST_NEXT(ifma, ifma_link)) {
		IFMA_LOCK_SPIN(ifma);
		if (!sa_equal(sa, ifma->ifma_addr)) {
			IFMA_UNLOCK(ifma);
			continue;
		}
		if (anon) {
			VERIFY(!(ifma->ifma_flags & IFMAF_ANONYMOUS) ||
			    ifma->ifma_anoncnt != 0);
			VERIFY((ifma->ifma_flags & IFMAF_ANONYMOUS) ||
			    ifma->ifma_anoncnt == 0);
			ifma->ifma_anoncnt++;
			if (!(ifma->ifma_flags & IFMAF_ANONYMOUS)) {
				VERIFY(ifma->ifma_anoncnt == 1);
				ifma->ifma_flags |= IFMAF_ANONYMOUS;
			}
		}
		if (!anon || ifma->ifma_anoncnt == 1) {
			ifma->ifma_reqcnt++;
			VERIFY(ifma->ifma_reqcnt > 1);
		}
		if (retifma != NULL) {
			*retifma = ifma;
			IFMA_ADDREF_LOCKED(ifma);
		}
		IFMA_UNLOCK(ifma);
		return 0;
	}
	return ENOENT;
}

/*
 * Radar 3642395, make sure all multicasts are in a standard format.
 */
static struct sockaddr *
copy_and_normalize(const struct sockaddr *original)
{
	int                     alen = 0;
	const u_char            *aptr = NULL;
	struct sockaddr         *__single copy = NULL;
	struct sockaddr_dl      *__single sdl_new = NULL;
	int                     len = 0;

	if (original->sa_family != AF_LINK &&
	    original->sa_family != AF_UNSPEC) {
		/* Just make a copy */
		copy = SA(kalloc_data(original->sa_len, Z_WAITOK));
		if (copy != NULL) {
			SOCKADDR_COPY(original, copy, original->sa_len);
		}
		return copy;
	}

	switch (original->sa_family) {
	case AF_LINK: {
		const struct sockaddr_dl *sdl_original =
		    SDL(original);

		if (sdl_original->sdl_len < offsetof(struct sockaddr_dl, sdl_data)) {
			return NULL;
		}
		if (sdl_original->sdl_nlen + sdl_original->sdl_alen +
		    sdl_original->sdl_slen +
		    offsetof(struct sockaddr_dl, sdl_data) >
		    sdl_original->sdl_len) {
			return NULL;
		}

		alen = sdl_original->sdl_alen;
		aptr = CONST_LLADDR(sdl_original);
	}
	break;

	case AF_UNSPEC: {
		if (original->sa_len < ETHER_ADDR_LEN +
		    offsetof(struct sockaddr, sa_data)) {
			return NULL;
		}

		alen = ETHER_ADDR_LEN;
		aptr = (const u_char *)original->sa_data;
	}
	break;
	}

	if (alen == 0 || aptr == NULL) {
		return NULL;
	}

	/*
	 * Ensure that we always allocate at least `sizeof(struct sockaddr_dl)' bytes
	 * to avoid bounds-safety related issues later.
	 */
	len = MAX(alen + offsetof(struct sockaddr_dl, sdl_data), sizeof(struct sockaddr_dl));
	sdl_new = SDL(kalloc_data(len, Z_WAITOK | Z_ZERO));

	if (sdl_new != NULL) {
		sdl_new->sdl_len = (u_char)len;
		sdl_new->sdl_family = AF_LINK;
		sdl_new->sdl_alen = (u_char)alen;
		bcopy(aptr, LLADDR(sdl_new), alen);
	}

	return SA(sdl_new);
}

/*
 * Network-layer protocol domains which hold references to the underlying
 * link-layer record must use this routine.
 */
int
if_addmulti(struct ifnet *ifp, const struct sockaddr *sa,
    struct ifmultiaddr **retifma)
{
	return if_addmulti_common(ifp, sa, retifma, 0);
}

/*
 * Anything other than network-layer protocol domains which hold references
 * to the underlying link-layer record must use this routine: SIOCADDMULTI
 * ioctl, ifnet_add_multicast(), if_bond.
 */
int
if_addmulti_anon(struct ifnet *ifp, const struct sockaddr *sa,
    struct ifmultiaddr **retifma)
{
	return if_addmulti_common(ifp, sa, retifma, 1);
}

/*
 * Register an additional multicast address with a network interface.
 *
 * - If the address is already present, bump the reference count on the
 *   address and return.
 * - If the address is not link-layer, look up a link layer address.
 * - Allocate address structures for one or both addresses, and attach to the
 *   multicast address list on the interface.  If automatically adding a link
 *   layer address, the protocol address will own a reference to the link
 *   layer address, to be freed when it is freed.
 * - Notify the network device driver of an addition to the multicast address
 *   list.
 *
 * 'sa' points to caller-owned memory with the desired multicast address.
 *
 * 'retifma' will be used to return a pointer to the resulting multicast
 * address reference, if desired.
 *
 * 'anon' indicates a link-layer address with no protocol address reference
 * made to it.  Anything other than network-layer protocol domain requests
 * are considered as anonymous.
 */
static int
if_addmulti_common(struct ifnet *ifp, const struct sockaddr *sa,
    struct ifmultiaddr **retifma, int anon)
{
	struct sockaddr_storage storage;
	struct sockaddr *__single llsa = NULL;
	struct sockaddr *__single dupsa = NULL;
	int error = 0, ll_firstref = 0, lladdr;
	struct ifmultiaddr *__single ifma = NULL;
	struct ifmultiaddr *__single llifma = NULL;

	/* Only AF_UNSPEC/AF_LINK is allowed for an "anonymous" address */
	VERIFY(!anon || sa->sa_family == AF_UNSPEC ||
	    sa->sa_family == AF_LINK);

	/* If sa is a AF_LINK or AF_UNSPEC, duplicate and normalize it */
	if (sa->sa_family == AF_LINK || sa->sa_family == AF_UNSPEC) {
		dupsa = copy_and_normalize(sa);
		if (dupsa == NULL) {
			error = ENOMEM;
			goto cleanup;
		}
		sa = dupsa;
	}

	ifnet_lock_exclusive(ifp);
	if (!(ifp->if_flags & IFF_MULTICAST)) {
		error = EADDRNOTAVAIL;
		ifnet_lock_done(ifp);
		goto cleanup;
	}

	/* If the address is already present, return a new reference to it */
	error = if_addmulti_doesexist(ifp, sa, retifma, anon);
	ifnet_lock_done(ifp);
	if (error == 0) {
		goto cleanup;
	}

	/*
	 * The address isn't already present; give the link layer a chance
	 * to accept/reject it, and also find out which AF_LINK address this
	 * maps to, if it isn't one already.
	 */
	error = dlil_resolve_multi(ifp, sa, SA(&storage),
	    sizeof(storage));
	if (error == 0 && storage.ss_len != 0) {
		llsa = copy_and_normalize(SA(&storage));
		if (llsa == NULL) {
			error = ENOMEM;
			goto cleanup;
		}

		llifma = ifma_alloc(Z_WAITOK);
		if (llifma == NULL) {
			error = ENOMEM;
			goto cleanup;
		}
	}

	/* to be similar to FreeBSD */
	if (error == EOPNOTSUPP) {
		error = 0;
	} else if (error != 0) {
		goto cleanup;
	}

	/* Allocate while we aren't holding any locks */
	if (dupsa == NULL) {
		dupsa = copy_and_normalize(sa);
		if (dupsa == NULL) {
			error = ENOMEM;
			goto cleanup;
		}
	}
	ifma = ifma_alloc(Z_WAITOK);
	if (ifma == NULL) {
		error = ENOMEM;
		goto cleanup;
	}

	ifnet_lock_exclusive(ifp);
	/*
	 * Check again for the matching multicast.
	 */
	error = if_addmulti_doesexist(ifp, sa, retifma, anon);
	if (error == 0) {
		ifnet_lock_done(ifp);
		goto cleanup;
	}

	if (llifma != NULL) {
		VERIFY(!anon);  /* must not get here if "anonymous" */
		if (if_addmulti_doesexist(ifp, llsa, &ifma->ifma_ll, 0) == 0) {
			kfree_data(llsa, llsa->sa_len);
			llsa = NULL;
			ifma_free(llifma);
			llifma = NULL;
			VERIFY(ifma->ifma_ll->ifma_ifp == ifp);
		} else {
			ll_firstref = 1;
			llifma->ifma_addr = llsa;
			llifma->ifma_ifp = ifp;
			IFMA_LOCK(llifma);
			if_attach_ifma(ifp, llifma, 0);
			/* add extra refcnt for ifma */
			IFMA_ADDREF_LOCKED(llifma);
			IFMA_UNLOCK(llifma);
			ifma->ifma_ll = llifma;
		}
	}

	/* "anonymous" request should not result in network address */
	VERIFY(!anon || ifma->ifma_ll == NULL);

	ifma->ifma_addr = dupsa;
	ifma->ifma_ifp = ifp;
	IFMA_LOCK(ifma);
	if_attach_ifma(ifp, ifma, anon);
	IFMA_ADDREF_LOCKED(ifma);               /* for this routine */
	if (retifma != NULL) {
		*retifma = ifma;
		IFMA_ADDREF_LOCKED(*retifma);   /* for caller */
	}
	lladdr = (ifma->ifma_addr->sa_family == AF_UNSPEC ||
	    ifma->ifma_addr->sa_family == AF_LINK);
	IFMA_UNLOCK(ifma);
	ifnet_lock_done(ifp);

	rt_newmaddrmsg(RTM_NEWMADDR, ifma);
	IFMA_REMREF(ifma);                      /* for this routine */

	/*
	 * We are certain we have added something, so call down to the
	 * interface to let them know about it.  Do this only for newly-
	 * added AF_LINK/AF_UNSPEC address in the if_multiaddrs set.
	 * Note that the notification is deferred to avoid
	 * locking reodering issues in certain paths.
	 */
	if (lladdr || ll_firstref) {
		ifnet_ioctl_async(ifp, SIOCADDMULTI);
	}

	if (ifp->if_updatemcasts > 0) {
		ifp->if_updatemcasts = 0;
	}

	return 0;

cleanup:
	if (ifma != NULL) {
		ifma_free(ifma);
	}
	if (dupsa != NULL) {
		kfree_data(dupsa, dupsa->sa_len);
	}
	if (llifma != NULL) {
		ifma_free(llifma);
	}
	if (llsa != NULL) {
		kfree_data(llsa, llsa->sa_len);
	}

	return error;
}

/*
 * Delete a multicast group membership by network-layer group address.
 * This routine is deprecated.
 */
int
if_delmulti(struct ifnet *ifp, const struct sockaddr *sa)
{
	return if_delmulti_common(NULL, ifp, sa, 0);
}

/*
 * Delete a multicast group membership by group membership pointer.
 * Network-layer protocol domains must use this routine.
 */
int
if_delmulti_ifma(struct ifmultiaddr *ifma)
{
	return if_delmulti_common(ifma, NULL, NULL, 0);
}

/*
 * Anything other than network-layer protocol domains which hold references
 * to the underlying link-layer record must use this routine: SIOCDELMULTI
 * ioctl, ifnet_remove_multicast(), if_bond.
 */
int
if_delmulti_anon(struct ifnet *ifp, const struct sockaddr *sa)
{
	return if_delmulti_common(NULL, ifp, sa, 1);
}

/*
 * Delete a multicast group membership by network-layer group address.
 *
 * Returns ENOENT if the entry could not be found.
 */
static int
if_delmulti_common(struct ifmultiaddr *ifma, struct ifnet *ifp,
    const struct sockaddr *sa, int anon)
{
	struct sockaddr *__single dupsa = NULL;
	int lastref, ll_lastref = 0, lladdr;
	struct ifmultiaddr *__single ll = NULL;

	/* sanity check for callers */
	VERIFY(ifma != NULL || (ifp != NULL && sa != NULL));

	if (ifma != NULL) {
		ifp = ifma->ifma_ifp;
	}

	if (sa != NULL &&
	    (sa->sa_family == AF_LINK || sa->sa_family == AF_UNSPEC)) {
		dupsa = copy_and_normalize(sa);
		if (dupsa == NULL) {
			return ENOMEM;
		}
		sa = dupsa;
	}

	ifnet_lock_exclusive(ifp);
	if (ifma == NULL) {
		for (ifma = LIST_FIRST(&ifp->if_multiaddrs); ifma != NULL;
		    ifma = LIST_NEXT(ifma, ifma_link)) {
			IFMA_LOCK(ifma);
			if (!sa_equal(sa, ifma->ifma_addr) ||
			    (anon && !(ifma->ifma_flags & IFMAF_ANONYMOUS))) {
				VERIFY(!(ifma->ifma_flags & IFMAF_ANONYMOUS) ||
				    ifma->ifma_anoncnt != 0);
				IFMA_UNLOCK(ifma);
				continue;
			}
			/* found; keep it locked */
			break;
		}
		if (ifma == NULL) {
			if (dupsa != NULL) {
				kfree_data(dupsa, dupsa->sa_len);
			}
			ifnet_lock_done(ifp);
			return ENOENT;
		}
	} else {
		IFMA_LOCK(ifma);
	}
	IFMA_LOCK_ASSERT_HELD(ifma);
	IFMA_ADDREF_LOCKED(ifma);       /* for this routine */
	lastref = if_detach_ifma(ifp, ifma, anon);
	VERIFY(!lastref || (!(ifma->ifma_debug & IFD_ATTACHED) &&
	    ifma->ifma_reqcnt == 0));
	VERIFY(!anon || ifma->ifma_ll == NULL);
	ll = ifma->ifma_ll;
	lladdr = (ifma->ifma_addr->sa_family == AF_UNSPEC ||
	    ifma->ifma_addr->sa_family == AF_LINK);
	IFMA_UNLOCK(ifma);
	if (lastref && ll != NULL) {
		IFMA_LOCK(ll);
		ll_lastref = if_detach_ifma(ifp, ll, 0);
		IFMA_UNLOCK(ll);
	}
	ifnet_lock_done(ifp);

	if (lastref) {
		rt_newmaddrmsg(RTM_DELMADDR, ifma);
	}

	if ((ll == NULL && lastref && lladdr) || ll_lastref) {
		/*
		 * Make sure the interface driver is notified in the
		 * case of a link layer mcast group being left.  Do
		 * this only for a AF_LINK/AF_UNSPEC address that has
		 * been removed from the if_multiaddrs set.
		 * Note that the notification is deferred to avoid
		 * locking reodering issues in certain paths.
		 */
		ifnet_ioctl_async(ifp, SIOCDELMULTI);
	}

	if (lastref) {
		IFMA_REMREF(ifma);      /* for if_multiaddrs list */
	}
	if (ll_lastref) {
		IFMA_REMREF(ll);        /* for if_multiaddrs list */
	}
	IFMA_REMREF(ifma);              /* for this routine */
	if (dupsa != NULL) {
		kfree_data(dupsa, dupsa->sa_len);
	}

	return 0;
}

/*
 * Shutdown all network activity.  Used boot() when halting
 * system.
 */
int
if_down_all(void)
{
	u_int32_t       count;
	ifnet_t *__counted_by(count) ifp = NULL;
	u_int32_t       i;

	if (ifnet_list_get_all(IFNET_FAMILY_ANY, &ifp, &count) == 0) {
		for (i = 0; i < count; i++) {
			if_down(ifp[i]);
			dlil_proto_unplumb_all(ifp[i]);
		}
		ifnet_list_free_counted_by(ifp, count);
	}

	return 0;
}

/*
 * Delete Routes for a Network Interface
 *
 * Called for each routing entry via the rnh->rnh_walktree() call above
 * to delete all route entries referencing a detaching network interface.
 *
 * Arguments:
 *	rn	pointer to node in the routing table
 *	arg	argument passed to rnh->rnh_walktree() - detaching interface
 *
 * Returns:
 *	0	successful
 *	errno	failed - reason indicated
 *
 */
static int
if_rtdel(struct radix_node *rn, void *arg)
{
	struct rtentry  *rt = (struct rtentry *)rn;
	struct ifnet    *__single ifp = arg;
	int             err;

	if (rt == NULL) {
		return 0;
	}
	/*
	 * Checking against RTF_UP protects against walktree
	 * recursion problems with cloned routes.
	 */
	RT_LOCK(rt);
	if (rt->rt_ifp == ifp && (rt->rt_flags & RTF_UP)) {
		/*
		 * Safe to drop rt_lock and use rt_key, rt_gateway,
		 * since holding rnh_lock here prevents another thread
		 * from calling rt_setgate() on this route.
		 */
		RT_UNLOCK(rt);
		err = rtrequest_locked(RTM_DELETE, rt_key(rt), rt->rt_gateway,
		    rt_mask(rt), rt->rt_flags, NULL);
		if (err) {
			log(LOG_WARNING, "if_rtdel: error %d\n", err);
		}
	} else {
		RT_UNLOCK(rt);
	}
	return 0;
}

/*
 * Removes routing table reference to a given interface
 * for a given protocol family
 */
void
if_rtproto_del(struct ifnet *ifp, int protocol)
{
	struct radix_node_head  *rnh;

	if ((protocol <= AF_MAX) && (protocol >= 0) &&
	    ((rnh = rt_tables[protocol]) != NULL) && (ifp != NULL)) {
		lck_mtx_lock(rnh_lock);
		(void) rnh->rnh_walktree(rnh, if_rtdel, ifp);
		lck_mtx_unlock(rnh_lock);
	}
}

static int
if_rtmtu(struct radix_node *rn, void *arg)
{
	struct rtentry *rt = (struct rtentry *)rn;
	struct ifnet *__single ifp = arg;

	RT_LOCK(rt);
	if (rt->rt_ifp == ifp) {
		/*
		 * Update the MTU of this entry only if the MTU
		 * has not been locked (RTV_MTU is not set) and
		 * if it was non-zero to begin with.
		 */
		if (!(rt->rt_rmx.rmx_locks & RTV_MTU) && rt->rt_rmx.rmx_mtu) {
			rt->rt_rmx.rmx_mtu = ifp->if_mtu;
			if (rt_key(rt)->sa_family == AF_INET &&
			    INTF_ADJUST_MTU_FOR_CLAT46(ifp)) {
				rt->rt_rmx.rmx_mtu = IN6_LINKMTU(ifp);
				/* Further adjust the size for CLAT46 expansion */
				rt->rt_rmx.rmx_mtu -= CLAT46_HDR_EXPANSION_OVERHD;
			}
		}
	}
	RT_UNLOCK(rt);

	return 0;
}

/*
 * Update the MTU metric of all route entries in all protocol tables
 * associated with a particular interface; this is called when the
 * MTU of that interface has changed.
 */
static void
if_rtmtu_update(struct ifnet *ifp)
{
	struct radix_node_head *__single rnh;
	int p;

	for (p = 0; p < AF_MAX + 1; p++) {
		if ((rnh = rt_tables[p]) == NULL) {
			continue;
		}

		lck_mtx_lock(rnh_lock);
		(void) rnh->rnh_walktree(rnh, if_rtmtu, ifp);
		lck_mtx_unlock(rnh_lock);
	}
	routegenid_update();
}

__private_extern__ void
if_data_internal_to_if_data(struct ifnet *ifp,
    const struct if_data_internal *if_data_int, struct if_data *if_data)
{
#pragma unused(ifp)
#define COPYFIELD(fld)          if_data->fld = if_data_int->fld
#define COPYFIELD32(fld)        if_data->fld = (u_int32_t)(if_data_int->fld)
/* compiler will cast down to 32-bit */
#define COPYFIELD32_ATOMIC(fld) do {                                    \
	uint64_t _val = 0;                                              \
	_val = os_atomic_load((uint64_t *)(void *)(uintptr_t)&if_data_int->fld, relaxed); \
	if_data->fld = (uint32_t) _val;                                 \
} while (0)

	COPYFIELD(ifi_type);
	COPYFIELD(ifi_typelen);
	COPYFIELD(ifi_physical);
	COPYFIELD(ifi_addrlen);
	COPYFIELD(ifi_hdrlen);
	COPYFIELD(ifi_recvquota);
	COPYFIELD(ifi_xmitquota);
	if_data->ifi_unused1 = 0;
	COPYFIELD(ifi_mtu);
	COPYFIELD(ifi_metric);
	if (if_data_int->ifi_baudrate & 0xFFFFFFFF00000000LL) {
		if_data->ifi_baudrate = 0xFFFFFFFF;
	} else {
		COPYFIELD32(ifi_baudrate);
	}

	COPYFIELD32_ATOMIC(ifi_ipackets);
	COPYFIELD32_ATOMIC(ifi_ierrors);
	COPYFIELD32_ATOMIC(ifi_opackets);
	COPYFIELD32_ATOMIC(ifi_oerrors);
	COPYFIELD32_ATOMIC(ifi_collisions);
	COPYFIELD32_ATOMIC(ifi_ibytes);
	COPYFIELD32_ATOMIC(ifi_obytes);
	COPYFIELD32_ATOMIC(ifi_imcasts);
	COPYFIELD32_ATOMIC(ifi_omcasts);
	COPYFIELD32_ATOMIC(ifi_iqdrops);
	COPYFIELD32_ATOMIC(ifi_noproto);

	COPYFIELD(ifi_recvtiming);
	COPYFIELD(ifi_xmittiming);

	if_data->ifi_lastchange.tv_sec = (uint32_t)if_data_int->ifi_lastchange.tv_sec;
	if_data->ifi_lastchange.tv_usec = if_data_int->ifi_lastchange.tv_usec;

	if_data->ifi_lastchange.tv_sec += (uint32_t)boottime_sec();

	if_data->ifi_unused2 = 0;
	COPYFIELD(ifi_hwassist);
	if_data->ifi_reserved1 = 0;
	if_data->ifi_reserved2 = 0;
#undef COPYFIELD32_ATOMIC
#undef COPYFIELD32
#undef COPYFIELD
}

__private_extern__ void
if_data_internal_to_if_data64(struct ifnet *ifp,
    const struct if_data_internal *if_data_int,
    struct if_data64 *if_data64)
{
#pragma unused(ifp)
#define COPYFIELD64(fld)        if_data64->fld = if_data_int->fld
#define COPYFIELD64_ATOMIC(fld) do {                                    \
	if_data64->fld = os_atomic_load((uint64_t *)(void *)(uintptr_t)&if_data_int->fld, relaxed); \
} while (0)

	COPYFIELD64(ifi_type);
	COPYFIELD64(ifi_typelen);
	COPYFIELD64(ifi_physical);
	COPYFIELD64(ifi_addrlen);
	COPYFIELD64(ifi_hdrlen);
	COPYFIELD64(ifi_recvquota);
	COPYFIELD64(ifi_xmitquota);
	if_data64->ifi_unused1 = 0;
	COPYFIELD64(ifi_mtu);
	COPYFIELD64(ifi_metric);
	COPYFIELD64(ifi_baudrate);

	COPYFIELD64_ATOMIC(ifi_ipackets);
	COPYFIELD64_ATOMIC(ifi_ierrors);
	COPYFIELD64_ATOMIC(ifi_opackets);
	COPYFIELD64_ATOMIC(ifi_oerrors);
	COPYFIELD64_ATOMIC(ifi_collisions);
	COPYFIELD64_ATOMIC(ifi_ibytes);
	COPYFIELD64_ATOMIC(ifi_obytes);
	COPYFIELD64_ATOMIC(ifi_imcasts);
	COPYFIELD64_ATOMIC(ifi_omcasts);
	COPYFIELD64_ATOMIC(ifi_iqdrops);
	COPYFIELD64_ATOMIC(ifi_noproto);

	/*
	 * Note these two fields are actually 32 bit, so doing
	 * COPYFIELD64_ATOMIC will cause them to be misaligned
	 */
	COPYFIELD64(ifi_recvtiming);
	COPYFIELD64(ifi_xmittiming);

	if_data64->ifi_lastchange.tv_sec = (uint32_t)if_data_int->ifi_lastchange.tv_sec;
	if_data64->ifi_lastchange.tv_usec = (uint32_t)if_data_int->ifi_lastchange.tv_usec;

	if_data64->ifi_lastchange.tv_sec += (uint32_t)boottime_sec();

#undef COPYFIELD64
}

__private_extern__ void
if_copy_traffic_class(struct ifnet *ifp,
    struct if_traffic_class *if_tc)
{
#define COPY_IF_TC_FIELD64_ATOMIC(fld) do {                     \
	if_tc->fld = os_atomic_load((uint64_t *)(void *)(uintptr_t)&ifp->if_tc.fld, relaxed); \
} while (0)

	bzero(if_tc, sizeof(*if_tc));
	COPY_IF_TC_FIELD64_ATOMIC(ifi_ibepackets);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_ibebytes);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_obepackets);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_obebytes);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_ibkpackets);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_ibkbytes);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_obkpackets);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_obkbytes);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_ivipackets);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_ivibytes);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_ovipackets);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_ovibytes);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_ivopackets);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_ivobytes);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_ovopackets);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_ovobytes);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_ipvpackets);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_ipvbytes);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_opvpackets);
	COPY_IF_TC_FIELD64_ATOMIC(ifi_opvbytes);

#undef COPY_IF_TC_FIELD64_ATOMIC
}

void
if_copy_data_extended(struct ifnet *ifp, struct if_data_extended *if_de)
{
#define COPY_IF_DE_FIELD64_ATOMIC(fld) do {                     \
	if_de->fld = os_atomic_load((uint64_t *)(void *)(uintptr_t)&ifp->if_data.fld, relaxed); \
} while (0)

	bzero(if_de, sizeof(*if_de));
	COPY_IF_DE_FIELD64_ATOMIC(ifi_alignerrs);
	COPY_IF_DE_FIELD64_ATOMIC(ifi_dt_bytes);
	COPY_IF_DE_FIELD64_ATOMIC(ifi_fpackets);
	COPY_IF_DE_FIELD64_ATOMIC(ifi_fbytes);

#undef COPY_IF_DE_FIELD64_ATOMIC
}

void
if_copy_packet_stats(struct ifnet *ifp, struct if_packet_stats *if_ps)
{
#define COPY_IF_PS_TCP_FIELD64_ATOMIC(fld) do {                         \
	if_ps->ifi_tcp_##fld = os_atomic_load((uint64_t *)(void *)(uintptr_t)&ifp->if_tcp_stat->fld, relaxed); \
} while (0)

#define COPY_IF_PS_UDP_FIELD64_ATOMIC(fld) do {                         \
	if_ps->ifi_udp_##fld = os_atomic_load((uint64_t *)(void *)(uintptr_t)&ifp->if_udp_stat->fld, relaxed); \
} while (0)

	COPY_IF_PS_TCP_FIELD64_ATOMIC(badformat);
	COPY_IF_PS_TCP_FIELD64_ATOMIC(unspecv6);
	COPY_IF_PS_TCP_FIELD64_ATOMIC(synfin);
	COPY_IF_PS_TCP_FIELD64_ATOMIC(badformatipsec);
	COPY_IF_PS_TCP_FIELD64_ATOMIC(noconnnolist);
	COPY_IF_PS_TCP_FIELD64_ATOMIC(noconnlist);
	COPY_IF_PS_TCP_FIELD64_ATOMIC(listbadsyn);
	COPY_IF_PS_TCP_FIELD64_ATOMIC(icmp6unreach);
	COPY_IF_PS_TCP_FIELD64_ATOMIC(deprecate6);
	COPY_IF_PS_TCP_FIELD64_ATOMIC(ooopacket);
	COPY_IF_PS_TCP_FIELD64_ATOMIC(rstinsynrcv);
	COPY_IF_PS_TCP_FIELD64_ATOMIC(dospacket);
	COPY_IF_PS_TCP_FIELD64_ATOMIC(cleanup);
	COPY_IF_PS_TCP_FIELD64_ATOMIC(synwindow);

	COPY_IF_PS_UDP_FIELD64_ATOMIC(port_unreach);
	COPY_IF_PS_UDP_FIELD64_ATOMIC(faithprefix);
	COPY_IF_PS_UDP_FIELD64_ATOMIC(port0);
	COPY_IF_PS_UDP_FIELD64_ATOMIC(badlength);
	COPY_IF_PS_UDP_FIELD64_ATOMIC(badchksum);
	COPY_IF_PS_UDP_FIELD64_ATOMIC(badmcast);
	COPY_IF_PS_UDP_FIELD64_ATOMIC(cleanup);
	COPY_IF_PS_UDP_FIELD64_ATOMIC(badipsec);

#undef COPY_IF_PS_TCP_FIELD64_ATOMIC
#undef COPY_IF_PS_UDP_FIELD64_ATOMIC
}

void
if_copy_rxpoll_stats(struct ifnet *ifp, struct if_rxpoll_stats *if_rs)
{
	bzero(if_rs, sizeof(*if_rs));
	if (!(ifp->if_eflags & IFEF_RXPOLL) || !ifnet_is_attached(ifp, 1)) {
		return;
	}
	bcopy(&ifp->if_poll_pstats, if_rs, sizeof(*if_rs));
	/* Release the IO refcnt */
	ifnet_decr_iorefcnt(ifp);
}

void
if_copy_netif_stats(struct ifnet *ifp, struct if_netif_stats *if_ns)
{
	bzero(if_ns, sizeof(*if_ns));
#if SKYWALK
	if (!(ifp->if_capabilities & IFCAP_SKYWALK) ||
	    !ifnet_is_attached(ifp, 1)) {
		return;
	}

	if (ifp->if_na != NULL) {
		nx_netif_copy_stats(ifp->if_na, if_ns);
	}

	/* Release the IO refcnt */
	ifnet_decr_iorefcnt(ifp);
#else /* SKYWALK */
#pragma unused(ifp)
#endif /* SKYWALK */
}

void
ifa_deallocated(struct ifaddr *ifa)
{
	IFA_LOCK_SPIN(ifa);

	if (__improbable(ifa->ifa_debug & IFD_ATTACHED)) {
		panic("ifa %p attached to ifp is being freed", ifa);
	}
	/*
	 * Some interface addresses are allocated either statically
	 * or carved out of a larger block.  Only free it if it was
	 * allocated via MALLOC or via the corresponding per-address
	 * family allocator.  Otherwise, leave it alone.
	 */
	if (ifa->ifa_debug & IFD_ALLOC) {
#if XNU_PLATFORM_MacOSX
		if (ifa->ifa_free == NULL) {
			IFA_UNLOCK(ifa);
			/*
			 * support for 3rd party kexts,
			 * old ABI was that this had to be allocated
			 * with MALLOC(M_IFADDR).
			 */
			__typed_allocators_ignore(kheap_free_addr(KHEAP_DEFAULT, ifa));
		} else
#endif /* XNU_PLATFORM_MacOSX */
		{
			/* Become a regular mutex */
			IFA_CONVERT_LOCK(ifa);
			/* callee will unlock */
			(*ifa->ifa_free)(ifa);
		}
	} else {
		IFA_UNLOCK(ifa);
	}
}

void
ifa_initref(struct ifaddr *ifa)
{
	os_ref_init_raw(&ifa->ifa_refcnt, &ifa_refgrp);
}

void
ifa_lock_init(struct ifaddr *ifa)
{
	lck_mtx_init(&ifa->ifa_lock, &ifa_mtx_grp, &ifa_mtx_attr);
}

void
ifa_lock_destroy(struct ifaddr *ifa)
{
	IFA_LOCK_ASSERT_NOTHELD(ifa);
	lck_mtx_destroy(&ifa->ifa_lock, &ifa_mtx_grp);
}

/*
 * 'i' group ioctls.
 *
 * The switch statement below does nothing at runtime, as it serves as a
 * compile time check to ensure that all of the socket 'i' ioctls (those
 * in the 'i' group going thru soo_ioctl) that are made available by the
 * networking stack is unique.  This works as long as this routine gets
 * updated each time a new interface ioctl gets added.
 *
 * Any failures at compile time indicates duplicated ioctl values.
 */
static __attribute__((unused)) void
ifioctl_cassert(void)
{
	/*
	 * This is equivalent to _CASSERT() and the compiler wouldn't
	 * generate any instructions, thus for compile time only.
	 */
	switch ((u_long)0) {
	case 0:

	/* bsd/net/if_ppp.h */
	case SIOCGPPPSTATS:
	case SIOCGPPPCSTATS:

	/* bsd/netinet6/in6_var.h */
	case SIOCSIFADDR_IN6:
	case SIOCGIFADDR_IN6:
	case SIOCSIFDSTADDR_IN6:
	case SIOCSIFNETMASK_IN6:
	case SIOCGIFDSTADDR_IN6:
	case SIOCGIFNETMASK_IN6:
	case SIOCDIFADDR_IN6:
	case SIOCAIFADDR_IN6_32:
	case SIOCAIFADDR_IN6_64:
	case SIOCSIFPHYADDR_IN6_32:
	case SIOCSIFPHYADDR_IN6_64:
	case SIOCGIFPSRCADDR_IN6:
	case SIOCGIFPDSTADDR_IN6:
	case SIOCGIFAFLAG_IN6:
	case OSIOCGIFINFO_IN6:
	case SIOCGIFINFO_IN6:
	case SIOCSNDFLUSH_IN6:
	case SIOCGNBRINFO_IN6_32:
	case SIOCGNBRINFO_IN6_64:
	case SIOCSPFXFLUSH_IN6:
	case SIOCSRTRFLUSH_IN6:
	case SIOCGIFALIFETIME_IN6:
	case SIOCSIFALIFETIME_IN6:
	case SIOCGIFSTAT_IN6:
	case SIOCGIFSTAT_ICMP6:
	case SIOCSDEFIFACE_IN6_32:
	case SIOCSDEFIFACE_IN6_64:
	case SIOCGDEFIFACE_IN6_32:
	case SIOCGDEFIFACE_IN6_64:
	case SIOCSIFINFO_FLAGS:
	case SIOCSSCOPE6:
	case SIOCGSCOPE6:
	case SIOCGSCOPE6DEF:
	case SIOCSIFPREFIX_IN6:
	case SIOCGIFPREFIX_IN6:
	case SIOCDIFPREFIX_IN6:
	case SIOCAIFPREFIX_IN6:
	case SIOCCIFPREFIX_IN6:
	case SIOCSGIFPREFIX_IN6:
	case SIOCPROTOATTACH_IN6_32:
	case SIOCPROTOATTACH_IN6_64:
	case SIOCPROTODETACH_IN6:
	case SIOCLL_START_32:
	case SIOCLL_START_64:
	case SIOCLL_STOP:
	case SIOCAUTOCONF_START:
	case SIOCAUTOCONF_STOP:
	case SIOCSETROUTERMODE_IN6:
	case SIOCGETROUTERMODE_IN6:
	case SIOCLL_CGASTART_32:
	case SIOCLL_CGASTART_64:
	case SIOCGIFCGAPREP_IN6:
	case SIOCSIFCGAPREP_IN6:

	/* bsd/sys/sockio.h */
	case SIOCSIFADDR:
	case OSIOCGIFADDR:
	case SIOCSIFDSTADDR:
	case OSIOCGIFDSTADDR:
	case SIOCSIFFLAGS:
	case SIOCGIFFLAGS:
	case OSIOCGIFBRDADDR:
	case SIOCSIFBRDADDR:
	case OSIOCGIFCONF32:
	case OSIOCGIFCONF64:
	case OSIOCGIFNETMASK:
	case SIOCSIFNETMASK:
	case SIOCGIFMETRIC:
	case SIOCSIFMETRIC:
	case SIOCDIFADDR:
	case SIOCAIFADDR:

	case SIOCGIFADDR:
	case SIOCGIFDSTADDR:
	case SIOCGIFBRDADDR:
	case SIOCGIFCONF32:
	case SIOCGIFCONF64:
	case SIOCGIFNETMASK:
	case SIOCAUTOADDR:
	case SIOCAUTONETMASK:
	case SIOCARPIPLL:

	case SIOCADDMULTI:
	case SIOCDELMULTI:
	case SIOCGIFMTU:
	case SIOCSIFMTU:
	case SIOCGIFPHYS:
	case SIOCSIFPHYS:
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA32:
	case SIOCGIFMEDIA64:
	case SIOCGIFXMEDIA32:
	case SIOCGIFXMEDIA64:
	case SIOCSIFGENERIC:
	case SIOCGIFGENERIC:
	case SIOCRSLVMULTI:

	case SIOCSIFLLADDR:
	case SIOCGIFSTATUS:
	case SIOCSIFPHYADDR:
	case SIOCGIFPSRCADDR:
	case SIOCGIFPDSTADDR:
	case SIOCDIFPHYADDR:

	case SIOCGIFDEVMTU:
	case SIOCSIFALTMTU:
	case SIOCGIFALTMTU:
	case SIOCSIFBOND:
	case SIOCGIFBOND:

	case SIOCPROTOATTACH:
	case SIOCPROTODETACH:

	case SIOCSIFCAP:
	case SIOCGIFCAP:

	case SIOCSIFMANAGEMENT:
	case SIOCSATTACHPROTONULL:

	case SIOCIFCREATE:
	case SIOCIFDESTROY:
	case SIOCIFCREATE2:

	case SIOCSDRVSPEC32:
	case SIOCGDRVSPEC32:
	case SIOCSDRVSPEC64:
	case SIOCGDRVSPEC64:

	case SIOCSIFVLAN:
	case SIOCGIFVLAN:

	case SIOCIFGCLONERS32:
	case SIOCIFGCLONERS64:

	case SIOCGIFASYNCMAP:
	case SIOCSIFASYNCMAP:
	case SIOCSIFKPI:
	case SIOCGIFKPI:

	case SIOCGIFWAKEFLAGS:

	case SIOCGIFGETRTREFCNT:
	case SIOCGIFLINKQUALITYMETRIC:
	case SIOCSIFLINKQUALITYMETRIC:
	case SIOCSIFOPPORTUNISTIC:
	case SIOCGIFOPPORTUNISTIC:
	case SIOCGETROUTERMODE:
	case SIOCSETROUTERMODE:
	case SIOCGIFEFLAGS:
	case SIOCSIFDESC:
	case SIOCGIFDESC:
	case SIOCSIFLINKPARAMS:
	case SIOCGIFLINKPARAMS:
	case SIOCGIFQUEUESTATS:
	case SIOCSIFTHROTTLE:
	case SIOCGIFTHROTTLE:

	case SIOCGASSOCIDS32:
	case SIOCGASSOCIDS64:
	case SIOCGCONNIDS32:
	case SIOCGCONNIDS64:
	case SIOCGCONNINFO32:
	case SIOCGCONNINFO64:
	case SIOCSCONNORDER:
	case SIOCGCONNORDER:

	case SIOCSIFLOG:
	case SIOCGIFLOG:
	case SIOCGIFDELEGATE:
	case SIOCGIFLLADDR:
	case SIOCGIFTYPE:
	case SIOCGIFEXPENSIVE:
	case SIOCSIFEXPENSIVE:
	case SIOCGIF2KCL:
	case SIOCSIF2KCL:
	case SIOCGSTARTDELAY:

	case SIOCAIFAGENTID:
	case SIOCDIFAGENTID:
	case SIOCGIFAGENTIDS32:
	case SIOCGIFAGENTIDS64:
	case SIOCGIFAGENTDATA32:
	case SIOCGIFAGENTDATA64:

	case SIOCSIFINTERFACESTATE:
	case SIOCGIFINTERFACESTATE:
	case SIOCSIFPROBECONNECTIVITY:
	case SIOCGIFPROBECONNECTIVITY:

	case SIOCGIFFUNCTIONALTYPE:
	case SIOCSIFPEEREGRESSFUNCTIONALTYPE:
	case SIOCSIFNETSIGNATURE:
	case SIOCGIFNETSIGNATURE:

	case SIOCSIFNETWORKID:
	case SIOCGECNMODE:
	case SIOCSECNMODE:

	case SIOCSIFORDER:
	case SIOCGIFORDER:

	case SIOCSQOSMARKINGMODE:
	case SIOCSQOSMARKINGENABLED:
	case SIOCGQOSMARKINGMODE:
	case SIOCGQOSMARKINGENABLED:

	case SIOCSIFTIMESTAMPENABLE:
	case SIOCSIFTIMESTAMPDISABLE:
	case SIOCGIFTIMESTAMPENABLED:

	case SIOCSIFDISABLEOUTPUT:

	case SIOCSIFSUBFAMILY:

	case SIOCGIFAGENTLIST32:
	case SIOCGIFAGENTLIST64:

	case SIOCSIFLOWINTERNET:
	case SIOCGIFLOWINTERNET:

	case SIOCGIFNAT64PREFIX:
	case SIOCSIFNAT64PREFIX:

	case SIOCGIFNEXUS:

	case SIOCGIFPROTOLIST32:
	case SIOCGIFPROTOLIST64:

	case SIOCGIFTCPKAOMAX:
	case SIOCGIFLOWPOWER:
	case SIOCSIFLOWPOWER:

	case SIOCGIFCLAT46ADDR:

	case SIOCGIFMPKLOG:
	case SIOCSIFMPKLOG:

	case SIOCGIFCONSTRAINED:
	case SIOCSIFCONSTRAINED:

	case SIOCGIFXFLAGS:

	case SIOCGIFNOACKPRIO:
	case SIOCSIFNOACKPRIO:

	case SIOCSIFMARKWAKEPKT:

	case SIOCSIFNOTRAFFICSHAPING:
	case SIOCGIFNOTRAFFICSHAPING:

	case SIOCGIFULTRACONSTRAINED:
	case SIOCSIFULTRACONSTRAINED:

	case SIOCSIFDIRECTLINK:
	case SIOCGIFDIRECTLINK:

	case SIOCSIFISVPN:

	case SIOCSIFDELAYWAKEPKTEVENT:
	case SIOCGIFDELAYWAKEPKTEVENT:

	case SIOCSIFDISABLEINPUT:
	case SIOCGIFDISABLEINPUT:
		;
	}
}

#if SKYWALK
/*
 * XXX: This API is only used by BSD stack and for now will always return 0.
 * For Skywalk native drivers, preamble space need not be allocated in mbuf
 * as the preamble will be reserved in the translated skywalk packet
 * which is transmitted to the driver.
 * For Skywalk compat drivers currently headroom is always set to zero.
 */
#endif /* SKYWALK */
uint32_t
ifnet_mbuf_packetpreamblelen(struct ifnet *ifp)
{
#pragma unused(ifp)
	return 0;
}

/* The following is used to enqueue work items for interface events */
struct intf_event {
	struct ifnet *ifp;
	union sockaddr_in_4_6 addr;
	uint32_t intf_event_code;
};

struct intf_event_nwk_wq_entry {
	struct nwk_wq_entry nwk_wqe;
	struct intf_event intf_ev_arg;
};

static void
intf_event_callback(struct nwk_wq_entry *nwk_item)
{
	struct intf_event_nwk_wq_entry *__single p_ev;

	p_ev = __container_of(nwk_item, struct intf_event_nwk_wq_entry, nwk_wqe);

	/* Call this before we walk the tree */
	EVENTHANDLER_INVOKE(&ifnet_evhdlr_ctxt, ifnet_event,
	    p_ev->intf_ev_arg.ifp,
	    SA(&(p_ev->intf_ev_arg.addr)),
	    p_ev->intf_ev_arg.intf_event_code);

	kfree_type(struct intf_event_nwk_wq_entry, p_ev);
}

void
intf_event_enqueue_nwk_wq_entry(struct ifnet *ifp, struct sockaddr *addrp,
    uint32_t intf_event_code)
{
#pragma unused(addrp)
	struct intf_event_nwk_wq_entry *__single p_intf_ev = NULL;

	p_intf_ev = kalloc_type(struct intf_event_nwk_wq_entry,
	    Z_WAITOK | Z_ZERO | Z_NOFAIL);

	p_intf_ev->intf_ev_arg.ifp = ifp;
	/*
	 * XXX Not using addr in the arg. This will be used
	 * once we need IP address add/delete events
	 */
	p_intf_ev->intf_ev_arg.intf_event_code = intf_event_code;
	p_intf_ev->nwk_wqe.func = intf_event_callback;

	evhlog(debug, "%s: eventhandler enqueuing event of type=intf_event event_code=%s",
	    __func__, intf_event2str(intf_event_code));

	nwk_wq_enqueue(&p_intf_ev->nwk_wqe);
}

const char *
intf_event2str(int intf_event)
{
	switch (intf_event) {
#define INTF_STATE_TO_STRING(type) case type: return #type;
		INTF_STATE_TO_STRING(INTF_EVENT_CODE_CREATED)
		INTF_STATE_TO_STRING(INTF_EVENT_CODE_REMOVED)
		INTF_STATE_TO_STRING(INTF_EVENT_CODE_STATUS_UPDATE)
		INTF_STATE_TO_STRING(INTF_EVENT_CODE_IPADDR_ATTACHED)
		INTF_STATE_TO_STRING(INTF_EVENT_CODE_IPADDR_DETACHED)
		INTF_STATE_TO_STRING(INTF_EVENT_CODE_LLADDR_UPDATE)
		INTF_STATE_TO_STRING(INTF_EVENT_CODE_MTU_CHANGED)
		INTF_STATE_TO_STRING(INTF_EVENT_CODE_LOW_POWER_UPDATE)
#undef INTF_STATE_TO_STRING
	}
	return "UNKNOWN_INTF_STATE";
}

int
if_get_tcp_kao_max(struct ifnet *ifp)
{
	int error = 0;

	if (ifp->if_tcp_kao_max == 0) {
		struct ifreq ifr;

		memset(&ifr, 0, sizeof(struct ifreq));
		error = ifnet_ioctl(ifp, 0, SIOCGIFTCPKAOMAX, &ifr);

		ifnet_lock_exclusive(ifp);
		if (error == 0) {
			ifp->if_tcp_kao_max = ifr.ifr_tcp_kao_max;
		} else if (error == EOPNOTSUPP) {
			ifp->if_tcp_kao_max = default_tcp_kao_max;
		}
		ifnet_lock_done(ifp);
	}
	return error;
}

int
ifnet_set_management(struct ifnet *ifp, boolean_t on)
{
	if (ifp == NULL) {
		return EINVAL;
	}
	if (if_management_verbose > 0) {
		os_log(OS_LOG_DEFAULT,
		    "interface %s management set %s by %s:%d",
		    ifp->if_xname, on ? "true" : "false",
		    proc_best_name(current_proc()), proc_selfpid());
	}
	if (on) {
		if_set_xflags(ifp, IFXF_MANAGEMENT);
		if_management_interface_check_needed = true;
		in_management_interface_check();
	} else {
		if_clear_xflags(ifp, IFXF_MANAGEMENT);
	}
	return 0;
}
