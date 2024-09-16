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
/*
 * Copyright (c) 1988, 1991, 1993
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
 *	@(#)rtsock.c	8.5 (Berkeley) 11/2/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kauth.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/syslog.h>
#include <sys/mcache.h>
#include <kern/locks.h>
#include <sys/codesign.h>

#include <net/if.h>
#include <net/route.h>
#include <net/dlil.h>
#include <net/raw_cb.h>
#include <net/net_sysctl.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_arp.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet6/nd6.h>

#include <net/sockaddr_utils.h>

#include <IOKit/IOBSD.h>

extern struct rtstat_64 rtstat;
extern struct domain routedomain_s;
static struct domain *routedomain = NULL;

static struct sockaddr route_dst = { .sa_len = 2, .sa_family = PF_ROUTE, .sa_data = { 0, } };
static struct sockaddr route_src = { .sa_len = 2, .sa_family = PF_ROUTE, .sa_data = { 0, } };
static struct sockaddr sa_zero   = { .sa_len = sizeof(sa_zero), .sa_family = AF_INET, .sa_data = { 0, } };

struct route_cb {
	u_int32_t       ip_count;       /* attached w/ AF_INET */
	u_int32_t       ip6_count;      /* attached w/ AF_INET6 */
	u_int32_t       any_count;      /* total attached */
};

static struct route_cb route_cb;

struct walkarg {
	int     w_tmemsize;
	int     w_op, w_arg;
	caddr_t w_tmem __sized_by(w_tmemsize);
	struct sysctl_req *w_req;
};

typedef struct walkarg * __single walkarg_ref_t;

static void route_dinit(struct domain *);
static int rts_abort(struct socket *);
static int rts_attach(struct socket *, int, struct proc *);
static int rts_bind(struct socket *, struct sockaddr *, struct proc *);
static int rts_connect(struct socket *, struct sockaddr *, struct proc *);
static int rts_detach(struct socket *);
static int rts_disconnect(struct socket *);
static int rts_peeraddr(struct socket *, struct sockaddr **);
static int rts_send(struct socket *, int, struct mbuf *, struct sockaddr *,
    struct mbuf *, struct proc *);
static int rts_shutdown(struct socket *);
static int rts_sockaddr(struct socket *, struct sockaddr **);

static int route_output(struct mbuf *, struct socket *);
static int rt_setmetrics(u_int32_t, struct rt_metrics *, struct rtentry *);
static void rt_getmetrics(struct rtentry *, struct rt_metrics *);
static void rt_setif(struct rtentry *, struct sockaddr *, struct sockaddr *,
    struct sockaddr *, unsigned int);
static int rt_xaddrs(caddr_t cp __ended_by(cplim), caddr_t cplim, struct rt_addrinfo *rtinfo, struct sockaddr xtra_storage[RTAX_MAX]);
static struct mbuf *rt_msg1(u_char, struct rt_addrinfo *);
static int rt_msg2(u_char, struct rt_addrinfo *, caddr_t __indexable, struct walkarg *,
    kauth_cred_t *);
static int sysctl_dumpentry(struct radix_node *rn, void *vw);
static int sysctl_dumpentry_ext(struct radix_node *rn, void *vw);
static int sysctl_iflist(int af, struct walkarg *w);
static int sysctl_iflist2(int af, struct walkarg *w);
static int sysctl_rtstat(struct sysctl_req *);
static int sysctl_rtstat_64(struct sysctl_req *);
static int sysctl_rttrash(struct sysctl_req *);
static int sysctl_rtsock SYSCTL_HANDLER_ARGS;

SYSCTL_NODE(_net, PF_ROUTE, routetable, CTLFLAG_RD | CTLFLAG_LOCKED,
    sysctl_rtsock, "");

SYSCTL_NODE(_net, OID_AUTO, route, CTLFLAG_RW | CTLFLAG_LOCKED, 0, "routing");

/* Align x to 1024 (only power of 2) assuming x is positive */
#define ALIGN_BYTES(x) do {                                             \
	x = (uint32_t)P2ALIGN(x, 1024);                         \
} while(0)

#define ROUNDUP32(a)                                                    \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof (uint32_t) - 1))) :        \
	sizeof (uint32_t))


#define RT_HAS_IFADDR(rt)                                               \
	((rt)->rt_ifa != NULL && (rt)->rt_ifa->ifa_addr != NULL)

/*
 * It really doesn't make any sense at all for this code to share much
 * with raw_usrreq.c, since its functionality is so restricted.  XXX
 */
static int
rts_abort(struct socket *so)
{
	return raw_usrreqs.pru_abort(so);
}

/* pru_accept is EOPNOTSUPP */

static int
rts_attach(struct socket *so, int proto, struct proc *p)
{
#pragma unused(p)
	struct rawcb *rp;
	int error;

	VERIFY(so->so_pcb == NULL);

	rp = kalloc_type(struct rawcb, Z_WAITOK_ZERO_NOFAIL);
	so->so_pcb = (caddr_t)rp;
	/* don't use raw_usrreqs.pru_attach, it checks for SS_PRIV */
	error = raw_attach(so, proto);
	rp = sotorawcb(so);
	if (error) {
		kfree_type(struct rawcb, rp);
		so->so_pcb = NULL;
		so->so_flags |= SOF_PCBCLEARING;
		return error;
	}

	switch (rp->rcb_proto.sp_protocol) {
	case AF_INET:
		os_atomic_inc(&route_cb.ip_count, relaxed);
		break;
	case AF_INET6:
		os_atomic_inc(&route_cb.ip6_count, relaxed);
		break;
	}
	rp->rcb_faddr = &route_src;
	os_atomic_inc(&route_cb.any_count, relaxed);
	/* the socket is already locked when we enter rts_attach */
	soisconnected(so);
	so->so_options |= SO_USELOOPBACK;
	return 0;
}

static int
rts_bind(struct socket *so, struct sockaddr *nam, struct proc *p)
{
	return raw_usrreqs.pru_bind(so, nam, p); /* xxx just EINVAL */
}

static int
rts_connect(struct socket *so, struct sockaddr *nam, struct proc *p)
{
	return raw_usrreqs.pru_connect(so, nam, p); /* XXX just EINVAL */
}

/* pru_connect2 is EOPNOTSUPP */
/* pru_control is EOPNOTSUPP */

static int
rts_detach(struct socket *so)
{
	struct rawcb *rp = sotorawcb(so);

	VERIFY(rp != NULL);

	switch (rp->rcb_proto.sp_protocol) {
	case AF_INET:
		os_atomic_dec(&route_cb.ip_count, relaxed);
		break;
	case AF_INET6:
		os_atomic_dec(&route_cb.ip6_count, relaxed);
		break;
	}
	os_atomic_dec(&route_cb.any_count, relaxed);
	return raw_usrreqs.pru_detach(so);
}

static int
rts_disconnect(struct socket *so)
{
	return raw_usrreqs.pru_disconnect(so);
}

/* pru_listen is EOPNOTSUPP */

static int
rts_peeraddr(struct socket *so, struct sockaddr **nam)
{
	return raw_usrreqs.pru_peeraddr(so, nam);
}

/* pru_rcvd is EOPNOTSUPP */
/* pru_rcvoob is EOPNOTSUPP */

static int
rts_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct proc *p)
{
	return raw_usrreqs.pru_send(so, flags, m, nam, control, p);
}

/* pru_sense is null */

static int
rts_shutdown(struct socket *so)
{
	return raw_usrreqs.pru_shutdown(so);
}

static int
rts_sockaddr(struct socket *so, struct sockaddr **nam)
{
	return raw_usrreqs.pru_sockaddr(so, nam);
}

static struct pr_usrreqs route_usrreqs = {
	.pru_abort =            rts_abort,
	.pru_attach =           rts_attach,
	.pru_bind =             rts_bind,
	.pru_connect =          rts_connect,
	.pru_detach =           rts_detach,
	.pru_disconnect =       rts_disconnect,
	.pru_peeraddr =         rts_peeraddr,
	.pru_send =             rts_send,
	.pru_shutdown =         rts_shutdown,
	.pru_sockaddr =         rts_sockaddr,
	.pru_sosend =           sosend,
	.pru_soreceive =        soreceive,
};

static struct rt_msghdr *
__attribute__((always_inline))
__stateful_pure
_rtm_hdr(caddr_t rtm_data __header_indexable)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-align"
	return (struct rt_msghdr*)rtm_data;
#pragma clang diagnostic pop
}

/*ARGSUSED*/
static int
route_output(struct mbuf *m, struct socket *so)
{
	size_t rtm_len = 0;
	caddr_t rtm_buf __counted_by(rtm_len) = NULL;
	caddr_t rtm_tmpbuf;
#define RTM _rtm_hdr(rtm_buf)
	rtentry_ref_t rt = NULL;
	rtentry_ref_t saved_nrt = NULL;
	struct radix_node_head *rnh;
	struct rt_addrinfo info;
	struct sockaddr    tiny_sa_storage[RTAX_MAX];
	int len, error = 0;
	sa_family_t dst_sa_family = 0;
	struct ifnet *ifp = NULL;
	struct sockaddr_in dst_in, gate_in;
	int sendonlytoself = 0;
	unsigned int ifscope = IFSCOPE_NONE;
	struct rawcb *rp = NULL;
	boolean_t is_router = FALSE;
#define senderr(e) { error = (e); goto flush; }
	if (m == NULL || ((m->m_len < sizeof(intptr_t)) &&
	    (m = m_pullup(m, sizeof(intptr_t))) == NULL)) {
		return ENOBUFS;
	}
	VERIFY(m->m_flags & M_PKTHDR);

	/*
	 * Unlock the socket (but keep a reference) it won't be
	 * accessed until raw_input appends to it.
	 */
	socket_unlock(so, 0);
	lck_mtx_lock(rnh_lock);

	len = m->m_pkthdr.len;
	if (len < sizeof(*RTM) ||
	    len != mtod(m, struct rt_msghdr_prelude *)->rtm_msglen) {
		info.rti_info[RTAX_DST] = NULL;
		senderr(EINVAL);
	}

	/*
	 * Allocate the buffer for the message. First we allocate
	 * a temporary buffer, and if successful, set the pointers.
	 */
	rtm_tmpbuf = kalloc_data(len, Z_WAITOK);
	if (rtm_tmpbuf == NULL) {
		info.rti_info[RTAX_DST] = NULL;
		senderr(ENOBUFS);
	}
	rtm_len = (size_t)len;
	rtm_buf = rtm_tmpbuf;
	rtm_tmpbuf = NULL;


	m_copydata(m, 0, len, rtm_buf);

	if (RTM->rtm_version != RTM_VERSION) {
		info.rti_info[RTAX_DST] = NULL;
		senderr(EPROTONOSUPPORT);
	}

	/*
	 * Silent version of RTM_GET for Reachabiltiy APIs. We may change
	 * all RTM_GETs to be silent in the future, so this is private for now.
	 */
	if (RTM->rtm_type == RTM_GET_SILENT) {
		if (!(so->so_options & SO_USELOOPBACK)) {
			senderr(EINVAL);
		}
		sendonlytoself = 1;
		RTM->rtm_type = RTM_GET;
	}

	/*
	 * Perform permission checking, only privileged sockets
	 * may perform operations other than RTM_GET
	 */
	if (RTM->rtm_type != RTM_GET && !(so->so_state & SS_PRIV)) {
		info.rti_info[RTAX_DST] = NULL;
		senderr(EPERM);
	}

	RTM->rtm_pid = proc_selfpid();
	info.rti_addrs = RTM->rtm_addrs;

	if (rt_xaddrs(rtm_buf + sizeof(struct rt_msghdr), rtm_buf + rtm_len, &info, tiny_sa_storage)) {
		info.rti_info[RTAX_DST] = NULL;
		senderr(EINVAL);
	}

	if (info.rti_info[RTAX_DST] == NULL ||
	    info.rti_info[RTAX_DST]->sa_family >= AF_MAX ||
	    (info.rti_info[RTAX_GATEWAY] != NULL &&
	    info.rti_info[RTAX_GATEWAY]->sa_family >= AF_MAX)) {
		senderr(EINVAL);
	}

	if (info.rti_info[RTAX_DST]->sa_family == AF_INET &&
	    info.rti_info[RTAX_DST]->sa_len != sizeof(struct sockaddr_in)) {
		/* At minimum, we need up to sin_addr */
		if (info.rti_info[RTAX_DST]->sa_len <
		    offsetof(struct sockaddr_in, sin_zero)) {
			senderr(EINVAL);
		}

		SOCKADDR_ZERO(&dst_in, sizeof(dst_in));
		dst_in.sin_len = sizeof(dst_in);
		dst_in.sin_family = AF_INET;
		dst_in.sin_port = SIN(info.rti_info[RTAX_DST])->sin_port;
		dst_in.sin_addr = SIN(info.rti_info[RTAX_DST])->sin_addr;
		info.rti_info[RTAX_DST] = SA(&dst_in);
		dst_sa_family = info.rti_info[RTAX_DST]->sa_family;
	} else if (info.rti_info[RTAX_DST]->sa_family == AF_INET6 &&
	    info.rti_info[RTAX_DST]->sa_len < sizeof(struct sockaddr_in6)) {
		senderr(EINVAL);
	}

	if (info.rti_info[RTAX_GATEWAY] != NULL) {
		if (info.rti_info[RTAX_GATEWAY]->sa_family == AF_INET &&
		    info.rti_info[RTAX_GATEWAY]->sa_len != sizeof(struct sockaddr_in)) {
			/* At minimum, we need up to sin_addr */
			if (info.rti_info[RTAX_GATEWAY]->sa_len <
			    offsetof(struct sockaddr_in, sin_zero)) {
				senderr(EINVAL);
			}

			SOCKADDR_ZERO(&gate_in, sizeof(gate_in));
			gate_in.sin_len = sizeof(gate_in);
			gate_in.sin_family = AF_INET;
			gate_in.sin_port = SIN(info.rti_info[RTAX_GATEWAY])->sin_port;
			gate_in.sin_addr = SIN(info.rti_info[RTAX_GATEWAY])->sin_addr;
			info.rti_info[RTAX_GATEWAY] = SA(&gate_in);
		} else if (info.rti_info[RTAX_GATEWAY]->sa_family == AF_INET6 &&
		    info.rti_info[RTAX_GATEWAY]->sa_len < sizeof(struct sockaddr_in6)) {
			senderr(EINVAL);
		}
	}

	if (info.rti_info[RTAX_GENMASK]) {
		struct radix_node *t;
		struct sockaddr *genmask = SA(info.rti_info[RTAX_GENMASK]);
		void *genmask_bytes = __SA_UTILS_CONV_TO_BYTES(genmask);
		t = rn_addmask(genmask_bytes, 0, 1);
		if (t != NULL && SOCKADDR_CMP(genmask, rn_get_key(t), genmask->sa_len) == 0) {
			info.rti_info[RTAX_GENMASK] = SA(rn_get_key(t));
		} else {
			senderr(ENOBUFS);
		}
	}

	/*
	 * If RTF_IFSCOPE flag is set, then rtm_index specifies the scope.
	 */
	if (RTM->rtm_flags & RTF_IFSCOPE) {
		if (info.rti_info[RTAX_DST]->sa_family != AF_INET &&
		    info.rti_info[RTAX_DST]->sa_family != AF_INET6) {
			senderr(EINVAL);
		}
		ifscope = RTM->rtm_index;
	}
	/*
	 * Block changes on INTCOPROC interfaces.
	 */
	if (ifscope != IFSCOPE_NONE) {
		unsigned int intcoproc_scope = 0;
		ifnet_head_lock_shared();
		TAILQ_FOREACH(ifp, &ifnet_head, if_link) {
			if (IFNET_IS_INTCOPROC(ifp)) {
				intcoproc_scope = ifp->if_index;
				break;
			}
		}
		ifnet_head_done();
		if (intcoproc_scope == ifscope && proc_getpid(current_proc()) != 0) {
			senderr(EINVAL);
		}
	}
	/*
	 * Require entitlement to change management interfaces
	 */
	if (management_control_unrestricted == false && if_management_interface_check_needed == true &&
	    ifscope != IFSCOPE_NONE && proc_getpid(current_proc()) != 0) {
		bool is_management = false;

		ifnet_head_lock_shared();
		if (IF_INDEX_IN_RANGE(ifscope)) {
			ifp = ifindex2ifnet[ifscope];
			if (ifp != NULL && IFNET_IS_MANAGEMENT(ifp)) {
				is_management = true;
			}
		}
		ifnet_head_done();

		if (is_management && !IOCurrentTaskHasEntitlement(MANAGEMENT_CONTROL_ENTITLEMENT)) {
			senderr(EINVAL);
		}
	}

	/*
	 * RTF_PROXY can only be set internally from within the kernel.
	 */
	if (RTM->rtm_flags & RTF_PROXY) {
		senderr(EINVAL);
	}

	/*
	 * For AF_INET, always zero out the embedded scope ID.  If this is
	 * a scoped request, it must be done explicitly by setting RTF_IFSCOPE
	 * flag and the corresponding rtm_index value.  This is to prevent
	 * false interpretation of the scope ID because it's using the sin_zero
	 * field, which might not be properly cleared by the requestor.
	 */
	if (info.rti_info[RTAX_DST]->sa_family == AF_INET) {
		sin_set_ifscope(info.rti_info[RTAX_DST], IFSCOPE_NONE);
	}
	if (info.rti_info[RTAX_GATEWAY] != NULL &&
	    info.rti_info[RTAX_GATEWAY]->sa_family == AF_INET) {
		sin_set_ifscope(info.rti_info[RTAX_GATEWAY], IFSCOPE_NONE);
	}
	if (info.rti_info[RTAX_DST]->sa_family == AF_INET6 &&
	    IN6_IS_SCOPE_EMBED(&SIN6(info.rti_info[RTAX_DST])->sin6_addr) &&
	    !IN6_IS_ADDR_UNICAST_BASED_MULTICAST(&SIN6(info.rti_info[RTAX_DST])->sin6_addr) &&
	    SIN6(info.rti_info[RTAX_DST])->sin6_scope_id == 0) {
		SIN6(info.rti_info[RTAX_DST])->sin6_scope_id = ntohs(SIN6(info.rti_info[RTAX_DST])->sin6_addr.s6_addr16[1]);
		SIN6(info.rti_info[RTAX_DST])->sin6_addr.s6_addr16[1] = 0;
	}

	switch (RTM->rtm_type) {
	case RTM_ADD:
		if (info.rti_info[RTAX_GATEWAY] == NULL) {
			senderr(EINVAL);
		}

		error = rtrequest_scoped_locked(RTM_ADD,
		    info.rti_info[RTAX_DST], info.rti_info[RTAX_GATEWAY],
		    info.rti_info[RTAX_NETMASK], RTM->rtm_flags, &saved_nrt,
		    ifscope);
		if (error == 0 && saved_nrt != NULL) {
			RT_LOCK(saved_nrt);
			/*
			 * If the route request specified an interface with
			 * IFA and/or IFP, we set the requested interface on
			 * the route with rt_setif.  It would be much better
			 * to do this inside rtrequest, but that would
			 * require passing the desired interface, in some
			 * form, to rtrequest.  Since rtrequest is called in
			 * so many places (roughly 40 in our source), adding
			 * a parameter is to much for us to swallow; this is
			 * something for the FreeBSD developers to tackle.
			 * Instead, we let rtrequest compute whatever
			 * interface it wants, then come in behind it and
			 * stick in the interface that we really want.  This
			 * works reasonably well except when rtrequest can't
			 * figure out what interface to use (with
			 * ifa_withroute) and returns ENETUNREACH.  Ideally
			 * it shouldn't matter if rtrequest can't figure out
			 * the interface if we're going to explicitly set it
			 * ourselves anyway.  But practically we can't
			 * recover here because rtrequest will not do any of
			 * the work necessary to add the route if it can't
			 * find an interface.  As long as there is a default
			 * route that leads to some interface, rtrequest will
			 * find an interface, so this problem should be
			 * rarely encountered.
			 * dwiggins@bbn.com
			 */
			rt_setif(saved_nrt,
			    info.rti_info[RTAX_IFP], info.rti_info[RTAX_IFA],
			    info.rti_info[RTAX_GATEWAY], ifscope);
			(void)rt_setmetrics(RTM->rtm_inits, &RTM->rtm_rmx, saved_nrt);
			saved_nrt->rt_rmx.rmx_locks &= ~(RTM->rtm_inits);
			saved_nrt->rt_rmx.rmx_locks |=
			    (RTM->rtm_inits & RTM->rtm_rmx.rmx_locks);
			saved_nrt->rt_genmask = info.rti_info[RTAX_GENMASK];
			RT_REMREF_LOCKED(saved_nrt);
			RT_UNLOCK(saved_nrt);
		}
		break;

	case RTM_DELETE:
		error = rtrequest_scoped_locked(RTM_DELETE,
		    info.rti_info[RTAX_DST], info.rti_info[RTAX_GATEWAY],
		    info.rti_info[RTAX_NETMASK], RTM->rtm_flags, &saved_nrt,
		    ifscope);
		if (error == 0) {
			rt = saved_nrt;
			RT_LOCK(rt);
			goto report;
		}
		break;

	case RTM_GET:
	case RTM_CHANGE:
	case RTM_LOCK:
		rnh = rt_tables[info.rti_info[RTAX_DST]->sa_family];
		if (rnh == NULL) {
			senderr(EAFNOSUPPORT);
		}
		/*
		 * Lookup the best match based on the key-mask pair;
		 * callee adds a reference and checks for root node.
		 */
		rt = rt_lookup(TRUE, info.rti_info[RTAX_DST],
		    info.rti_info[RTAX_NETMASK], rnh, ifscope);
		if (rt == NULL) {
			senderr(ESRCH);
		}
		RT_LOCK(rt);

		/*
		 * Holding rnh_lock here prevents the possibility of
		 * ifa from changing (e.g. in_ifinit), so it is safe
		 * to access its ifa_addr (down below) without locking.
		 */
		switch (RTM->rtm_type) {
		case RTM_GET: {
			kauth_cred_t cred __single;
			kauth_cred_t* credp;
			struct ifaddr *ifa2;
			/*
			 * The code below serves both the `RTM_GET'
			 * and the `RTM_DELETE' requests.
			 */
report:
			cred = current_cached_proc_cred(PROC_NULL);
			credp = &cred;

			ifa2 = NULL;
			RT_LOCK_ASSERT_HELD(rt);
			info.rti_info[RTAX_DST] = rt_key(rt);
			dst_sa_family = info.rti_info[RTAX_DST]->sa_family;
			info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
			info.rti_info[RTAX_NETMASK] = rt_mask(rt);
			info.rti_info[RTAX_GENMASK] = rt->rt_genmask;
			if (RTM->rtm_addrs & (RTA_IFP | RTA_IFA)) {
				ifp = rt->rt_ifp;
				if (ifp != NULL) {
					ifnet_lock_shared(ifp);
					ifa2 = ifp->if_lladdr;
					info.rti_info[RTAX_IFP] = ifa2->ifa_addr;
					ifa_addref(ifa2);
					ifnet_lock_done(ifp);
					info.rti_info[RTAX_IFA] = rt->rt_ifa->ifa_addr;
					RTM->rtm_index = ifp->if_index;
				} else {
					info.rti_info[RTAX_IFP] = NULL;
					info.rti_info[RTAX_IFA] = NULL;
				}
			} else if ((ifp = rt->rt_ifp) != NULL) {
				RTM->rtm_index = ifp->if_index;
			}

			/*
			 * Determine the length required for the routing information
			 * report.
			 */
			if (ifa2 != NULL) {
				IFA_LOCK(ifa2);
			}
			len = rt_msg2(RTM->rtm_type, &info, NULL, NULL, credp);
			if (ifa2 != NULL) {
				IFA_UNLOCK(ifa2);
			}

			/*
			 * Allocate output message for the routing information report.
			 */
			VERIFY(rtm_tmpbuf == NULL);
			rtm_tmpbuf = kalloc_data(len, Z_WAITOK);
			if (rtm_tmpbuf == NULL) {
				RT_UNLOCK(rt);
				if (ifa2 != NULL) {
					ifa_remref(ifa2);
				}
				senderr(ENOBUFS);
			}

			/*
			 * Create the header for the output message, based
			 * on the request message header and the current routing information.
			 */
			struct rt_msghdr *out_rtm = _rtm_hdr(rtm_tmpbuf);
			bcopy(RTM, out_rtm, sizeof(struct rt_msghdr));
			out_rtm->rtm_flags = rt->rt_flags;
			rt_getmetrics(rt, &out_rtm->rtm_rmx);
			out_rtm->rtm_addrs = info.rti_addrs;

			/*
			 * Populate the body of the output message.
			 */
			if (ifa2 != NULL) {
				IFA_LOCK(ifa2);
			}
			(void) rt_msg2(out_rtm->rtm_type, &info, rtm_tmpbuf,
			    NULL, &cred);
			if (ifa2 != NULL) {
				IFA_UNLOCK(ifa2);
			}

			/*
			 * Replace the "main" routing message with the output message
			 * we have constructed.
			 */
			kfree_data_counted_by(rtm_buf, rtm_len);
			rtm_len = len;
			rtm_buf = rtm_tmpbuf;
			rtm_tmpbuf = NULL;

			if (ifa2 != NULL) {
				ifa_remref(ifa2);
			}

			break;
		}

		case RTM_CHANGE:
			is_router = (rt->rt_flags & RTF_ROUTER) ? TRUE : FALSE;

			if (info.rti_info[RTAX_GATEWAY] != NULL &&
			    (error = rt_setgate(rt, rt_key(rt),
			    info.rti_info[RTAX_GATEWAY]))) {
				int tmp = error;
				RT_UNLOCK(rt);
				senderr(tmp);
			}
			/*
			 * If they tried to change things but didn't specify
			 * the required gateway, then just use the old one.
			 * This can happen if the user tries to change the
			 * flags on the default route without changing the
			 * default gateway. Changing flags still doesn't work.
			 */
			if ((rt->rt_flags & RTF_GATEWAY) &&
			    info.rti_info[RTAX_GATEWAY] == NULL) {
				info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
			}

			/*
			 * On Darwin, we call rt_setif which contains the
			 * equivalent to the code found at this very spot
			 * in BSD.
			 */
			rt_setif(rt,
			    info.rti_info[RTAX_IFP], info.rti_info[RTAX_IFA],
			    info.rti_info[RTAX_GATEWAY], ifscope);

			if ((error = rt_setmetrics(RTM->rtm_inits,
			    &RTM->rtm_rmx, rt))) {
				int tmp = error;
				RT_UNLOCK(rt);
				senderr(tmp);
			}
			if (info.rti_info[RTAX_GENMASK]) {
				rt->rt_genmask = info.rti_info[RTAX_GENMASK];
			}

			/*
			 * Enqueue work item to invoke callback for this route entry
			 * This may not be needed always, but for now issue it anytime
			 * RTM_CHANGE gets called.
			 */
			route_event_enqueue_nwk_wq_entry(rt, NULL, ROUTE_ENTRY_REFRESH, NULL, TRUE);
			/*
			 * If the route is for a router, walk the tree to send refresh
			 * event to protocol cloned entries
			 */
			if (is_router) {
				struct route_event rt_ev;
				route_event_init(&rt_ev, rt, NULL, ROUTE_ENTRY_REFRESH);
				RT_UNLOCK(rt);
				(void) rnh->rnh_walktree(rnh, route_event_walktree, (void *)&rt_ev);
				RT_LOCK(rt);
			}
			OS_FALLTHROUGH;
		case RTM_LOCK:
			rt->rt_rmx.rmx_locks &= ~(RTM->rtm_inits);
			rt->rt_rmx.rmx_locks |=
			    (RTM->rtm_inits & RTM->rtm_rmx.rmx_locks);
			break;
		}
		RT_UNLOCK(rt);
		break;
	default:
		senderr(EOPNOTSUPP);
	}
flush:
	if (RTM != NULL) {
		if (error) {
			RTM->rtm_errno = error;
		} else {
			RTM->rtm_flags |= RTF_DONE;
		}
	}
	if (rt != NULL) {
		RT_LOCK_ASSERT_NOTHELD(rt);
		rtfree_locked(rt);
	}
	lck_mtx_unlock(rnh_lock);

	/* relock the socket now */
	socket_lock(so, 0);
	/*
	 * Check to see if we don't want our own messages.
	 */
	if (!(so->so_options & SO_USELOOPBACK)) {
		if (route_cb.any_count <= 1) {
			kfree_data_counted_by(rtm_buf, rtm_len);
			m_freem(m);
			return error;
		}
		/* There is another listener, so construct message */
		rp = sotorawcb(so);
	}
	if (rtm_buf != NULL) {
		m_copyback(m, 0, RTM->rtm_msglen, rtm_buf);
		if (m->m_pkthdr.len < RTM->rtm_msglen) {
			m_freem(m);
			m = NULL;
		} else if (m->m_pkthdr.len > RTM->rtm_msglen) {
			m_adj(m, RTM->rtm_msglen - m->m_pkthdr.len);
		}
		kfree_data_counted_by(rtm_buf, rtm_len);
	}
	if (sendonlytoself && m != NULL) {
		error = 0;
		if (sbappendaddr(&so->so_rcv, &route_src, m,
		    NULL, &error) != 0) {
			sorwakeup(so);
		}
		if (error) {
			return error;
		}
	} else {
		struct sockproto route_proto = { .sp_family = PF_ROUTE, .sp_protocol = 0 };
		if (rp != NULL) {
			rp->rcb_proto.sp_family = 0; /* Avoid us */
		}
		if (dst_sa_family != 0) {
			route_proto.sp_protocol = dst_sa_family;
		}
		if (m != NULL) {
			socket_unlock(so, 0);
			raw_input(m, &route_proto, &route_src, &route_dst);
			socket_lock(so, 0);
		}
		if (rp != NULL) {
			rp->rcb_proto.sp_family = PF_ROUTE;
		}
	}
	return error;
#undef RTM /* was defined to __rtm_hdr(rtm_buf) */
}

void
rt_setexpire(struct rtentry *rt, uint64_t expiry)
{
	/* set both rt_expire and rmx_expire */
	rt->rt_expire = expiry;
	if (expiry) {
		rt->rt_rmx.rmx_expire =
		    (int32_t)(expiry + rt->base_calendartime -
		    rt->base_uptime);
	} else {
		rt->rt_rmx.rmx_expire = 0;
	}
}

static int
rt_setmetrics(u_int32_t which, struct rt_metrics *in, struct rtentry *out)
{
	if (!(which & RTV_REFRESH_HOST)) {
		struct timeval caltime;
		getmicrotime(&caltime);
#define metric(f, e) if (which & (f)) out->rt_rmx.e = in->e;
		metric(RTV_RPIPE, rmx_recvpipe);
		metric(RTV_SPIPE, rmx_sendpipe);
		metric(RTV_SSTHRESH, rmx_ssthresh);
		metric(RTV_RTT, rmx_rtt);
		metric(RTV_RTTVAR, rmx_rttvar);
		metric(RTV_HOPCOUNT, rmx_hopcount);
		metric(RTV_MTU, rmx_mtu);
		metric(RTV_EXPIRE, rmx_expire);
#undef metric
		if (out->rt_rmx.rmx_expire > 0) {
			/* account for system time change */
			getmicrotime(&caltime);
			out->base_calendartime +=
			    NET_CALCULATE_CLOCKSKEW(caltime,
			    out->base_calendartime,
			    net_uptime(), out->base_uptime);
			rt_setexpire(out,
			    out->rt_rmx.rmx_expire -
			    out->base_calendartime +
			    out->base_uptime);
		} else {
			rt_setexpire(out, 0);
		}

		VERIFY(out->rt_expire == 0 || out->rt_rmx.rmx_expire != 0);
		VERIFY(out->rt_expire != 0 || out->rt_rmx.rmx_expire == 0);
	} else {
		/* Only RTV_REFRESH_HOST must be set */
		if ((which & ~RTV_REFRESH_HOST) ||
		    (out->rt_flags & RTF_STATIC) ||
		    !(out->rt_flags & RTF_LLINFO)) {
			return EINVAL;
		}

		if (out->rt_llinfo_refresh == NULL) {
			return ENOTSUP;
		}

		out->rt_llinfo_refresh(out);
	}
	return 0;
}

static void
rt_getmetrics(struct rtentry *in, struct rt_metrics *out)
{
	struct timeval caltime;

	VERIFY(in->rt_expire == 0 || in->rt_rmx.rmx_expire != 0);
	VERIFY(in->rt_expire != 0 || in->rt_rmx.rmx_expire == 0);

	*out = in->rt_rmx;

	if (in->rt_expire != 0) {
		/* account for system time change */
		getmicrotime(&caltime);

		in->base_calendartime +=
		    NET_CALCULATE_CLOCKSKEW(caltime,
		    in->base_calendartime, net_uptime(), in->base_uptime);

		out->rmx_expire = (int32_t)(in->base_calendartime +
		    in->rt_expire - in->base_uptime);
	} else {
		out->rmx_expire = 0;
	}
}

/*
 * Set route's interface given info.rti_info[RTAX_IFP],
 * info.rti_info[RTAX_IFA], and gateway.
 */
static void
rt_setif(struct rtentry *rt, struct sockaddr *Ifpaddr, struct sockaddr *Ifaaddr,
    struct sockaddr *Gate, unsigned int ifscope)
{
	struct ifaddr *ifa = NULL;
	struct ifnet *ifp = NULL;
	void (*ifa_rtrequest)(int, struct rtentry *, struct sockaddr *);

	LCK_MTX_ASSERT(rnh_lock, LCK_MTX_ASSERT_OWNED);

	RT_LOCK_ASSERT_HELD(rt);

	/* Don't update a defunct route */
	if (rt->rt_flags & RTF_CONDEMNED) {
		return;
	}

	/* Add an extra ref for ourselves */
	RT_ADDREF_LOCKED(rt);

	/* Become a regular mutex, just in case */
	RT_CONVERT_LOCK(rt);

	/*
	 * New gateway could require new ifaddr, ifp; flags may also
	 * be different; ifp may be specified by ll sockaddr when
	 * protocol address is ambiguous.
	 */
	if (Ifpaddr && (ifa = ifa_ifwithnet_scoped(Ifpaddr, ifscope)) &&
	    (ifp = ifa->ifa_ifp) && (Ifaaddr || Gate)) {
		ifa_remref(ifa);
		ifa = ifaof_ifpforaddr(Ifaaddr ? Ifaaddr : Gate, ifp);
	} else {
		if (ifa != NULL) {
			ifa_remref(ifa);
			ifa = NULL;
		}
		if (Ifpaddr && (ifp = if_withname(Ifpaddr))) {
			if (Gate) {
				ifa = ifaof_ifpforaddr(Gate, ifp);
			} else {
				ifnet_lock_shared(ifp);
				ifa = TAILQ_FIRST(&ifp->if_addrhead);
				if (ifa != NULL) {
					ifa_addref(ifa);
				}
				ifnet_lock_done(ifp);
			}
		} else if (Ifaaddr &&
		    (ifa = ifa_ifwithaddr_scoped(Ifaaddr, ifscope))) {
			ifp = ifa->ifa_ifp;
		} else if (Gate != NULL) {
			/*
			 * Safe to drop rt_lock and use rt_key, since holding
			 * rnh_lock here prevents another thread from calling
			 * rt_setgate() on this route.  We cannot hold the
			 * lock across ifa_ifwithroute since the lookup done
			 * by that routine may point to the same route.
			 */
			RT_UNLOCK(rt);
			if ((ifa = ifa_ifwithroute_scoped_locked(rt->rt_flags,
			    rt_key(rt), Gate, ifscope)) != NULL) {
				ifp = ifa->ifa_ifp;
			}
			RT_LOCK(rt);
			/* Don't update a defunct route */
			if (rt->rt_flags & RTF_CONDEMNED) {
				if (ifa != NULL) {
					ifa_remref(ifa);
				}
				/* Release extra ref */
				RT_REMREF_LOCKED(rt);
				return;
			}
		}
	}

	/* trigger route cache reevaluation */
	if (rt_key(rt)->sa_family == AF_INET) {
		routegenid_inet_update();
	} else if (rt_key(rt)->sa_family == AF_INET6) {
		routegenid_inet6_update();
	}

	if (ifa != NULL) {
		struct ifaddr *oifa = rt->rt_ifa;
		if (oifa != ifa) {
			if (oifa != NULL) {
				IFA_LOCK_SPIN(oifa);
				ifa_rtrequest = oifa->ifa_rtrequest;
				IFA_UNLOCK(oifa);
				if (ifa_rtrequest != NULL) {
					ifa_rtrequest(RTM_DELETE, rt, Gate);
				}
			}
			rtsetifa(rt, ifa);

			if (rt->rt_ifp != ifp) {
				/*
				 * Purge any link-layer info caching.
				 */
				if (rt->rt_llinfo_purge != NULL) {
					rt->rt_llinfo_purge(rt);
				}

				/*
				 * Adjust route ref count for the interfaces.
				 */
				if (rt->rt_if_ref_fn != NULL) {
					rt->rt_if_ref_fn(ifp, 1);
					rt->rt_if_ref_fn(rt->rt_ifp, -1);
				}
			}
			rt->rt_ifp = ifp;
			/*
			 * If this is the (non-scoped) default route, record
			 * the interface index used for the primary ifscope.
			 */
			if (rt_primary_default(rt, rt_key(rt))) {
				set_primary_ifscope(rt_key(rt)->sa_family,
				    rt->rt_ifp->if_index);
			}
			/*
			 * If rmx_mtu is not locked, update it
			 * to the MTU used by the new interface.
			 */
			if (!(rt->rt_rmx.rmx_locks & RTV_MTU)) {
				rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu;
				if (rt_key(rt)->sa_family == AF_INET &&
				    INTF_ADJUST_MTU_FOR_CLAT46(ifp)) {
					rt->rt_rmx.rmx_mtu = IN6_LINKMTU(rt->rt_ifp);
					/* Further adjust the size for CLAT46 expansion */
					rt->rt_rmx.rmx_mtu -= CLAT46_HDR_EXPANSION_OVERHD;
				}
			}

			if (rt->rt_ifa != NULL) {
				IFA_LOCK_SPIN(rt->rt_ifa);
				ifa_rtrequest = rt->rt_ifa->ifa_rtrequest;
				IFA_UNLOCK(rt->rt_ifa);
				if (ifa_rtrequest != NULL) {
					ifa_rtrequest(RTM_ADD, rt, Gate);
				}
			}
			ifa_remref(ifa);
			/* Release extra ref */
			RT_REMREF_LOCKED(rt);
			return;
		}
		ifa_remref(ifa);
		ifa = NULL;
	}

	/* XXX: to reset gateway to correct value, at RTM_CHANGE */
	if (rt->rt_ifa != NULL) {
		IFA_LOCK_SPIN(rt->rt_ifa);
		ifa_rtrequest = rt->rt_ifa->ifa_rtrequest;
		IFA_UNLOCK(rt->rt_ifa);
		if (ifa_rtrequest != NULL) {
			ifa_rtrequest(RTM_ADD, rt, Gate);
		}
	}

	/*
	 * Workaround for local address routes pointing to the loopback
	 * interface added by configd, until <rdar://problem/12970142>.
	 */
	if ((rt->rt_ifp->if_flags & IFF_LOOPBACK) &&
	    (rt->rt_flags & RTF_HOST) && rt->rt_ifa->ifa_ifp == rt->rt_ifp) {
		ifa = ifa_ifwithaddr(rt_key(rt));
		if (ifa != NULL) {
			if (ifa != rt->rt_ifa) {
				rtsetifa(rt, ifa);
			}
			ifa_remref(ifa);
		}
	}

	/* Release extra ref */
	RT_REMREF_LOCKED(rt);
}

/*
 * Extract the addresses of the passed sockaddrs.
 *
 * Do a little sanity checking so as to avoid bad memory references.
 * This data is derived straight from userland. Some of the data
 * anomalies are unrecoverable; for others we substitute the anomalous
 * user data with a sanitized replacement.
 *
 * Details on the input anomalies:
 *
 * 1. Unrecoverable input anomalies (retcode == EINVAL)
 *    The function returns EINVAL.
 *    1.1. Truncated sockaddrs at the end of the user-provided buffer.
 *    1.2. Unparseable sockaddr header (`0 < .sa_len && .sa_len < 2').
 *    1.3. Sockaddrs that won't fit `struct sockaddr_storage'.
 *
 * 2. Recoverable input anomalies (retcode == 0):
 *    The below anomalies would lead to a malformed `struct sockaddr *'
 *    pointers. Any attempt to pass such malformed pointers to a function
 *    or to assign those to another variable will cause a trap
 *    when the `-fbounds-safety' feature is enabled.
 *
 *    To mitigate the malformed pointers problem, we substitute the malformed
 *    user data with a well-formed sockaddrs.
 *
 *    2.1. Sockadrs with `.sa_len == 0' (aka "zero-length" sockaddrs).
 *         We substitute those with a pointer to the `sa_data' global
 *         variable.
 *    2.2. Sockaddrs with `.sa_len < 16' (a.k.a. "tiny" sockaddrs).
 *         We copy the contents of "tiny" sockaddrs to a location
 *         inside the `xtra_storage' parameter, and substitute
 *         the pointer into the user-provided data with the location
 *         in `xtra_storage'.
 */
static int
rt_xaddrs(caddr_t cp __ended_by(cplim), caddr_t cplim, struct rt_addrinfo *rtinfo, struct sockaddr xtra_storage[RTAX_MAX])
{
	struct sockaddr *sa;
	int i, next_tiny_sa = 0;

	for (i = 0; i < RTAX_MAX; i++) {
		SOCKADDR_ZERO(&xtra_storage[i], sizeof(struct sockaddr));
	}
	bzero(rtinfo->rti_info, sizeof(rtinfo->rti_info));

	for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
		if ((rtinfo->rti_addrs & (1 << i)) == 0) {
			continue;
		}

		/*
		 * We expect the memory pointed to by `cp' to contain a valid socket address.
		 * However, there are no guarantees that our expectations are correct,
		 * since the buffer is passed from the user-space.
		 * In particular, the socket address may be corrupted or truncated.
		 * If we attempt to interpret the contents of the memory pointed to by `cp'
		 * as a valid socket address, we may end up in a situation where the end
		 * of the presumed socket address exceeds the end of the input buffer:
		 *
		 * +-------------------------------+
		 * | user buffer                   |
		 * +-------------------------------+
		 *                       cp ^ cplim ^
		 *                          +-----------------------+
		 *                          | (struct sockaddr *)cp |
		 *                          +-----------------------+
		 *
		 * In such case, we are likely to panic with the `-fbounds-safety' trap,
		 * while the desired behavior is to return `ENOENT'.
		 *
		 * Because of the above concern, we can not optimistically cast the pointer
		 * `cp' to `struct sockaddr*' until we have validated that the contents
		 * of the memory can be safely interpreted as a socket address.
		 *
		 * Instead, we start by examining the expected length of the socket address,
		 * which is guaranteed to be located at the first byte, and perform several
		 * sanity checks, before interpreting the memory as a valid socket address.
		 */
		uint8_t next_sa_len = *cp;

		/*
		 * Is the user-provided sockaddr truncated?
		 */
		if ((cp + next_sa_len) > cplim) {
			return EINVAL;
		}

		/*
		 * Will the user-provided sockaddr fit the sockaddr storage?
		 */
		if (next_sa_len > sizeof(struct sockaddr_storage)) {
			return EINVAL;
		}

		/*
		 * there are no more.. quit now
		 * If there are more bits, they are in error.
		 * I've seen this. route(1) can evidently generate these.
		 * This causes kernel to core dump.
		 * for compatibility, If we see this, point to a safe address.
		 */
		if (next_sa_len == 0) {
			rtinfo->rti_info[i] = &sa_zero;
			return 0; /* should be EINVAL but for compat */
		}

		/*
		 * Check for the minimal length.
		 */
		if (next_sa_len < offsetof(struct sockaddr, sa_data)) {
			return EINVAL;
		}

		/*
		 * Check whether we are looking at a "tiny" sockaddr,
		 * and if so, copy the contents to the xtra storage.
		 * See the comment to this function for the details
		 * on "tiny" sockaddrs and the xtra storage.
		 */
		if (next_sa_len < sizeof(struct sockaddr)) {
			sa = &xtra_storage[next_tiny_sa++];
			SOCKADDR_COPY(cp, sa, next_sa_len);
		} else {
			sa = SA(cp);
		}

		/*
		 * From this point on we can safely use `sa'.
		 */

		/* accepthe  it */
		rtinfo->rti_info[i] = sa;
		const uint32_t rounded_sa_len = ROUNDUP32(sa->sa_len);
		if (cp + rounded_sa_len > cplim) {
			break;
		} else {
			cp += rounded_sa_len;
			cplim = cplim;
		}
	}
	return 0;
}

static struct mbuf *
rt_msg1(u_char type, struct rt_addrinfo *rtinfo)
{
	struct rt_msghdr_common *rtmh;
	int32_t *rtm_buf; /* int32 to preserve the alingment. */
	struct mbuf *m;
	int i;
	int len, dlen, off;

	switch (type) {
	case RTM_DELADDR:
	case RTM_NEWADDR:
		len = sizeof(struct ifa_msghdr);
		break;

	case RTM_DELMADDR:
	case RTM_NEWMADDR:
		len = sizeof(struct ifma_msghdr);
		break;

	case RTM_IFINFO:
		len = sizeof(struct if_msghdr);
		break;

	default:
		len = sizeof(struct rt_msghdr);
	}
	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m && len > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if (!(m->m_flags & M_EXT)) {
			m_free(m);
			m = NULL;
		}
	}
	if (m == NULL) {
		return NULL;
	}
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = NULL;
	rtm_buf = mtod(m, int32_t *);
	bzero(rtm_buf, len);
	off = len;
	for (i = 0; i < RTAX_MAX; i++) {
		struct sockaddr *sa, *hint;
		uint8_t ssbuf[SOCK_MAXADDRLEN + 1];

		/*
		 * Make sure to accomodate the largest possible size of sa_len.
		 */
		_CASSERT(sizeof(ssbuf) == (SOCK_MAXADDRLEN + 1));

		if ((sa = rtinfo->rti_info[i]) == NULL) {
			continue;
		}

		switch (i) {
		case RTAX_DST:
		case RTAX_NETMASK:
			if ((hint = rtinfo->rti_info[RTAX_DST]) == NULL) {
				hint = rtinfo->rti_info[RTAX_IFA];
			}

			/* Scrub away any trace of embedded interface scope */
			sa = rtm_scrub(type, i, hint, sa, &ssbuf,
			    sizeof(ssbuf), NULL);
			break;

		default:
			break;
		}

		rtinfo->rti_addrs |= (1 << i);
		dlen = sa->sa_len;
		m_copyback(m, off, dlen, __SA_UTILS_CONV_TO_BYTES(sa));
		len = off + dlen;
		off += ROUNDUP32(dlen);
	}
	if (m->m_pkthdr.len != len) {
		m_freem(m);
		return NULL;
	}
	rtmh = (struct rt_msghdr_common *)rtm_buf;
	rtmh->rtm_msglen = (u_short)len;
	rtmh->rtm_version = RTM_VERSION;
	rtmh->rtm_type = type;
	return m;
}

static int
rt_msg2(u_char type, struct rt_addrinfo *rtinfo, caddr_t cp __header_indexable, struct walkarg *w,
    kauth_cred_t* credp)
{
	int i;
	int len, dlen, rlen, second_time = 0;
	caddr_t cp0;

	rtinfo->rti_addrs = 0;
again:
	switch (type) {
	case RTM_DELADDR:
	case RTM_NEWADDR:
		len = sizeof(struct ifa_msghdr);
		break;

	case RTM_DELMADDR:
	case RTM_NEWMADDR:
		len = sizeof(struct ifma_msghdr);
		break;

	case RTM_IFINFO:
		len = sizeof(struct if_msghdr);
		break;

	case RTM_IFINFO2:
		len = sizeof(struct if_msghdr2);
		break;

	case RTM_NEWMADDR2:
		len = sizeof(struct ifma_msghdr2);
		break;

	case RTM_GET_EXT:
		len = sizeof(struct rt_msghdr_ext);
		break;

	case RTM_GET2:
		len = sizeof(struct rt_msghdr2);
		break;

	default:
		len = sizeof(struct rt_msghdr);
	}
	cp0 = cp;
	if (cp0) {
		cp += len;
	}
	for (i = 0; i < RTAX_MAX; i++) {
		struct sockaddr *sa, *hint;
		uint8_t ssbuf[SOCK_MAXADDRLEN + 1];

		/*
		 * Make sure to accomodate the largest possible size of sa_len.
		 */
		_CASSERT(sizeof(ssbuf) == (SOCK_MAXADDRLEN + 1));

		if ((sa = rtinfo->rti_info[i]) == NULL) {
			continue;
		}

		switch (i) {
		case RTAX_DST:
		case RTAX_NETMASK:
			if ((hint = rtinfo->rti_info[RTAX_DST]) == NULL) {
				hint = rtinfo->rti_info[RTAX_IFA];
			}

			/* Scrub away any trace of embedded interface scope */
			sa = rtm_scrub(type, i, hint, sa, &ssbuf,
			    sizeof(ssbuf), NULL);
			break;
		case RTAX_GATEWAY:
		case RTAX_IFP:
			sa = rtm_scrub(type, i, NULL, sa, &ssbuf,
			    sizeof(ssbuf), credp);
			break;

		default:
			break;
		}

		rtinfo->rti_addrs |= (1 << i);
		dlen = sa->sa_len;
		rlen = ROUNDUP32(dlen);
		if (cp) {
			SOCKADDR_COPY(sa, cp, dlen);
			if (dlen != rlen) {
				bzero(cp + dlen, rlen - dlen);
			}
			cp += rlen;
		}
		len += rlen;
	}
	if (cp == NULL && w != NULL && !second_time) {
		walkarg_ref_t rw = w;

		if (rw->w_req != NULL) {
			if (rw->w_tmemsize < len) {
				if (rw->w_tmem != NULL) {
					kfree_data_sized_by(rw->w_tmem, rw->w_tmemsize);
				}
				caddr_t new_tmem = (caddr_t)kalloc_data(len, Z_ZERO | Z_WAITOK);
				if (new_tmem != NULL) {
					rw->w_tmemsize = len;
					rw->w_tmem = new_tmem;
				}
			}
			if (rw->w_tmem != NULL) {
				cp = rw->w_tmem;
				second_time = 1;
				goto again;
			}
		}
	}
	if (cp) {
		struct rt_msghdr_common *rtmh = (struct rt_msghdr_common *)(void *)cp0;

		rtmh->rtm_version = RTM_VERSION;
		rtmh->rtm_type = type;
		rtmh->rtm_msglen = (u_short)len;
	}
	return len;
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that a redirect has occurred, a routing lookup
 * has failed, or that a protocol has detected timeouts to a particular
 * destination.
 */
void
rt_missmsg(u_char type, struct rt_addrinfo *rtinfo, int flags, int error)
{
	struct rt_msghdr_common *rtmh;
	struct mbuf *m;
	struct sockaddr *sa = rtinfo->rti_info[RTAX_DST];
	struct sockproto route_proto = { .sp_family = PF_ROUTE, .sp_protocol = 0 };

	if (route_cb.any_count == 0) {
		return;
	}
	m = rt_msg1(type, rtinfo);
	if (m == NULL) {
		return;
	}
	rtmh = mtod(m, struct rt_msghdr_common *);
	rtmh->rtm_flags = RTF_DONE | flags;
	rtmh->rtm_errno = error;
	rtmh->rtm_addrs = rtinfo->rti_addrs;
	route_proto.sp_family = sa ? sa->sa_family : 0;
	raw_input(m, &route_proto, &route_src, &route_dst);
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that the status of a network interface has changed.
 */
void
rt_ifmsg(struct ifnet *ifp)
{
	struct if_msghdr *ifm;
	struct mbuf *m;
	struct rt_addrinfo info;
	struct  sockproto route_proto = { .sp_family = PF_ROUTE, .sp_protocol = 0 };

	if (route_cb.any_count == 0) {
		return;
	}
	bzero((caddr_t)&info, sizeof(info));
	m = rt_msg1(RTM_IFINFO, &info);
	if (m == NULL) {
		return;
	}
	ifm = mtod(m, struct if_msghdr *);
	ifm->ifm_index = ifp->if_index;
	ifm->ifm_flags = (u_short)ifp->if_flags;
	if_data_internal_to_if_data(ifp, &ifp->if_data, &ifm->ifm_data);
	ifm->ifm_addrs = 0;
	raw_input(m, &route_proto, &route_src, &route_dst);
}

/*
 * This is called to generate messages from the routing socket
 * indicating a network interface has had addresses associated with it.
 * if we ever reverse the logic and replace messages TO the routing
 * socket indicate a request to configure interfaces, then it will
 * be unnecessary as the routing socket will automatically generate
 * copies of it.
 *
 * Since this is coming from the interface, it is expected that the
 * interface will be locked.  Caller must hold rnh_lock and rt_lock.
 */
void
rt_newaddrmsg(u_char cmd, struct ifaddr *ifa, int error, struct rtentry *rt)
{
	struct rt_addrinfo info;
	struct sockaddr *sa = 0;
	int pass;
	struct mbuf *m = 0;
	struct ifnet *ifp = ifa->ifa_ifp;
	struct sockproto route_proto = { .sp_family = PF_ROUTE, .sp_protocol = 0 };

	LCK_MTX_ASSERT(rnh_lock, LCK_MTX_ASSERT_OWNED);
	RT_LOCK_ASSERT_HELD(rt);

	if (route_cb.any_count == 0) {
		return;
	}

	/* Become a regular mutex, just in case */
	RT_CONVERT_LOCK(rt);
	for (pass = 1; pass < 3; pass++) {
		bzero((caddr_t)&info, sizeof(info));
		if ((cmd == RTM_ADD && pass == 1) ||
		    (cmd == RTM_DELETE && pass == 2)) {
			struct ifa_msghdr *ifam;
			u_char ncmd = cmd == RTM_ADD ? RTM_NEWADDR : RTM_DELADDR;

			/* Lock ifp for if_lladdr */
			ifnet_lock_shared(ifp);
			IFA_LOCK(ifa);
			info.rti_info[RTAX_IFA] = sa = ifa->ifa_addr;
			/*
			 * Holding ifnet lock here prevents the link address
			 * from changing contents, so no need to hold its
			 * lock.  The link address is always present; it's
			 * never freed.
			 */
			info.rti_info[RTAX_IFP] = ifp->if_lladdr->ifa_addr;
			info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;
			info.rti_info[RTAX_BRD] = ifa->ifa_dstaddr;
			if ((m = rt_msg1(ncmd, &info)) == NULL) {
				IFA_UNLOCK(ifa);
				ifnet_lock_done(ifp);
				continue;
			}
			IFA_UNLOCK(ifa);
			ifnet_lock_done(ifp);
			ifam = mtod(m, struct ifa_msghdr *);
			ifam->ifam_index = ifp->if_index;
			IFA_LOCK_SPIN(ifa);
			ifam->ifam_metric = ifa->ifa_metric;
			ifam->ifam_flags = ifa->ifa_flags;
			IFA_UNLOCK(ifa);
			ifam->ifam_addrs = info.rti_addrs;
		}
		if ((cmd == RTM_ADD && pass == 2) ||
		    (cmd == RTM_DELETE && pass == 1)) {
			struct rt_msghdr *rtm;

			if (rt == NULL) {
				continue;
			}
			info.rti_info[RTAX_NETMASK] = rt_mask(rt);
			info.rti_info[RTAX_DST] = sa = rt_key(rt);
			info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
			if ((m = rt_msg1(cmd, &info)) == NULL) {
				continue;
			}
			rtm = mtod(m, struct rt_msghdr *);
			rtm->rtm_index = ifp->if_index;
			rtm->rtm_flags |= rt->rt_flags;
			rtm->rtm_errno = error;
			rtm->rtm_addrs = info.rti_addrs;
		}
		route_proto.sp_protocol = sa ? sa->sa_family : 0;
		raw_input(m, &route_proto, &route_src, &route_dst);
	}
}

/*
 * This is the analogue to the rt_newaddrmsg which performs the same
 * function but for multicast group memberhips.  This is easier since
 * there is no route state to worry about.
 */
void
rt_newmaddrmsg(u_char cmd, struct ifmultiaddr *ifma)
{
	struct rt_addrinfo info;
	struct mbuf *m = 0;
	struct ifnet *ifp = ifma->ifma_ifp;
	struct ifma_msghdr *ifmam;
	struct sockproto route_proto = { .sp_family = PF_ROUTE, .sp_protocol = 0 };

	if (route_cb.any_count == 0) {
		return;
	}

	/* Lock ifp for if_lladdr */
	ifnet_lock_shared(ifp);
	bzero((caddr_t)&info, sizeof(info));
	IFMA_LOCK(ifma);
	info.rti_info[RTAX_IFA] = ifma->ifma_addr;
	/* lladdr doesn't need lock */
	info.rti_info[RTAX_IFP] = ifp->if_lladdr->ifa_addr;

	/*
	 * If a link-layer address is present, present it as a ``gateway''
	 * (similarly to how ARP entries, e.g., are presented).
	 */
	info.rti_info[RTAX_GATEWAY] = (ifma->ifma_ll != NULL) ?
	    ifma->ifma_ll->ifma_addr : NULL;
	if ((m = rt_msg1(cmd, &info)) == NULL) {
		IFMA_UNLOCK(ifma);
		ifnet_lock_done(ifp);
		return;
	}
	ifmam = mtod(m, struct ifma_msghdr *);
	ifmam->ifmam_index = ifp->if_index;
	ifmam->ifmam_addrs = info.rti_addrs;
	route_proto.sp_protocol = ifma->ifma_addr->sa_family;
	IFMA_UNLOCK(ifma);
	ifnet_lock_done(ifp);
	raw_input(m, &route_proto, &route_src, &route_dst);
}

const char *
rtm2str(int cmd)
{
	const char *c __null_terminated = "RTM_?";

	switch (cmd) {
	case RTM_ADD:
		c = "RTM_ADD";
		break;
	case RTM_DELETE:
		c = "RTM_DELETE";
		break;
	case RTM_CHANGE:
		c = "RTM_CHANGE";
		break;
	case RTM_GET:
		c = "RTM_GET";
		break;
	case RTM_LOSING:
		c = "RTM_LOSING";
		break;
	case RTM_REDIRECT:
		c = "RTM_REDIRECT";
		break;
	case RTM_MISS:
		c = "RTM_MISS";
		break;
	case RTM_LOCK:
		c = "RTM_LOCK";
		break;
	case RTM_OLDADD:
		c = "RTM_OLDADD";
		break;
	case RTM_OLDDEL:
		c = "RTM_OLDDEL";
		break;
	case RTM_RESOLVE:
		c = "RTM_RESOLVE";
		break;
	case RTM_NEWADDR:
		c = "RTM_NEWADDR";
		break;
	case RTM_DELADDR:
		c = "RTM_DELADDR";
		break;
	case RTM_IFINFO:
		c = "RTM_IFINFO";
		break;
	case RTM_NEWMADDR:
		c = "RTM_NEWMADDR";
		break;
	case RTM_DELMADDR:
		c = "RTM_DELMADDR";
		break;
	case RTM_GET_SILENT:
		c = "RTM_GET_SILENT";
		break;
	case RTM_IFINFO2:
		c = "RTM_IFINFO2";
		break;
	case RTM_NEWMADDR2:
		c = "RTM_NEWMADDR2";
		break;
	case RTM_GET2:
		c = "RTM_GET2";
		break;
	case RTM_GET_EXT:
		c = "RTM_GET_EXT";
		break;
	}

	return c;
}

/*
 * This is used in dumping the kernel table via sysctl().
 */
static int
sysctl_dumpentry(struct radix_node *rn, void *vw)
{
	walkarg_ref_t w = vw;
	rtentry_ref_t rt = rn_rtentry(rn);
	int error = 0, size;
	struct rt_addrinfo info;
	kauth_cred_t cred __single;
	kauth_cred_t *credp;

	cred = current_cached_proc_cred(PROC_NULL);
	credp = &cred;

	RT_LOCK(rt);
	if ((w->w_op == NET_RT_FLAGS || w->w_op == NET_RT_FLAGS_PRIV) &&
	    !(rt->rt_flags & w->w_arg)) {
		goto done;
	}

	/*
	 * If the matching route has RTF_LLINFO set, then we can skip scrubbing the MAC
	 * only if the outgoing interface is not loopback and the process has entitlement
	 * for neighbor cache read.
	 */
	if (w->w_op == NET_RT_FLAGS_PRIV && (rt->rt_flags & RTF_LLINFO)) {
		if (rt->rt_ifp != lo_ifp &&
		    (route_op_entitlement_check(NULL, cred, ROUTE_OP_READ, TRUE) == 0)) {
			credp = NULL;
		}
	}

	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_DST] = rt_key(rt);
	info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
	info.rti_info[RTAX_NETMASK] = rt_mask(rt);
	info.rti_info[RTAX_GENMASK] = rt->rt_genmask;
	if (RT_HAS_IFADDR(rt)) {
		info.rti_info[RTAX_IFA] = rt->rt_ifa->ifa_addr;
	}

	if (w->w_op != NET_RT_DUMP2) {
		size = rt_msg2(RTM_GET, &info, NULL, w, credp);
		if (w->w_req != NULL && w->w_tmem != NULL) {
			struct rt_msghdr *rtm =
			    (struct rt_msghdr *)(void *)w->w_tmem;

			rtm->rtm_flags = rt->rt_flags;
			rtm->rtm_use = rt->rt_use;
			rt_getmetrics(rt, &rtm->rtm_rmx);
			rtm->rtm_index = rt->rt_ifp->if_index;
			rtm->rtm_pid = 0;
			rtm->rtm_seq = 0;
			rtm->rtm_errno = 0;
			rtm->rtm_addrs = info.rti_addrs;
			error = SYSCTL_OUT(w->w_req, (caddr_t)rtm, size);
		}
	} else {
		size = rt_msg2(RTM_GET2, &info, NULL, w, credp);
		if (w->w_req != NULL && w->w_tmem != NULL) {
			struct rt_msghdr2 *rtm =
			    (struct rt_msghdr2 *)(void *)w->w_tmem;

			rtm->rtm_flags = rt->rt_flags;
			rtm->rtm_use = rt->rt_use;
			rt_getmetrics(rt, &rtm->rtm_rmx);
			rtm->rtm_index = rt->rt_ifp->if_index;
			rtm->rtm_refcnt = rt->rt_refcnt;
			if (rt->rt_parent) {
				rtm->rtm_parentflags = rt->rt_parent->rt_flags;
			} else {
				rtm->rtm_parentflags = 0;
			}
			rtm->rtm_reserved = 0;
			rtm->rtm_addrs = info.rti_addrs;
			error = SYSCTL_OUT(w->w_req, (caddr_t)rtm, size);
		}
	}

done:
	RT_UNLOCK(rt);
	return error;
}

/*
 * This is used for dumping extended information from route entries.
 */
static int
sysctl_dumpentry_ext(struct radix_node *rn, void *vw)
{
	walkarg_ref_t w = vw;
	rtentry_ref_t rt = rn_rtentry(rn);
	int error = 0, size;
	struct rt_addrinfo info;
	kauth_cred_t cred __single;

	cred = current_cached_proc_cred(PROC_NULL);

	RT_LOCK(rt);
	if (w->w_op == NET_RT_DUMPX_FLAGS && !(rt->rt_flags & w->w_arg)) {
		goto done;
	}
	bzero(&info, sizeof(info));
	info.rti_info[RTAX_DST] = rt_key(rt);
	info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
	info.rti_info[RTAX_NETMASK] = rt_mask(rt);
	info.rti_info[RTAX_GENMASK] = rt->rt_genmask;

	size = rt_msg2(RTM_GET_EXT, &info, NULL, w, &cred);
	if (w->w_req != NULL && w->w_tmem != NULL) {
		struct rt_msghdr_ext *ertm =
		    (struct rt_msghdr_ext *)(void *)w->w_tmem;

		ertm->rtm_flags = rt->rt_flags;
		ertm->rtm_use = rt->rt_use;
		rt_getmetrics(rt, &ertm->rtm_rmx);
		ertm->rtm_index = rt->rt_ifp->if_index;
		ertm->rtm_pid = 0;
		ertm->rtm_seq = 0;
		ertm->rtm_errno = 0;
		ertm->rtm_addrs = info.rti_addrs;
		if (rt->rt_llinfo_get_ri == NULL) {
			bzero(&ertm->rtm_ri, sizeof(ertm->rtm_ri));
			ertm->rtm_ri.ri_rssi = IFNET_RSSI_UNKNOWN;
			ertm->rtm_ri.ri_lqm = IFNET_LQM_THRESH_OFF;
			ertm->rtm_ri.ri_npm = IFNET_NPM_THRESH_UNKNOWN;
		} else {
			rt->rt_llinfo_get_ri(rt, &ertm->rtm_ri);
		}
		error = SYSCTL_OUT(w->w_req, (caddr_t)ertm, size);
	}

done:
	RT_UNLOCK(rt);
	return error;
}

static boolean_t
should_include_clat46(void)
{
#define CLAT46_ENTITLEMENT "com.apple.private.route.iflist.include-clat46"
	return IOCurrentTaskHasEntitlement(CLAT46_ENTITLEMENT);
}

static boolean_t
is_clat46_address(struct ifaddr *ifa)
{
	boolean_t       is_clat46 = FALSE;

	if (ifa->ifa_addr->sa_family == AF_INET6) {
		struct in6_ifaddr *ifa6 = ifatoia6(ifa);

		is_clat46 = (ifa6->ia6_flags & IN6_IFF_CLAT46) != 0;
	}
	return is_clat46;
}

/*
 * rdar://9307819
 * To avoid to call copyout() while holding locks and to cause problems
 * in the paging path, sysctl_iflist() and sysctl_iflist2() contstruct
 * the list in two passes. In the first pass we compute the total
 * length of the data we are going to copyout, then we release
 * all locks to allocate a temporary buffer that gets filled
 * in the second pass.
 *
 * Note that we are verifying the assumption that kalloc() returns a buffer
 * that is at least 32 bits aligned and that the messages and addresses are
 * 32 bits aligned.
 */
static int
sysctl_iflist(int af, struct walkarg *w)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct  rt_addrinfo info;
	int     error = 0;
	int     pass = 0;
	size_t  len = 0, total_len = 0, total_buffer_len = 0, current_len = 0;
	char    *total_buffer = NULL, *cp = NULL;
	kauth_cred_t cred __single;
	boolean_t include_clat46 = FALSE;
	boolean_t include_clat46_valid = FALSE;

	cred = current_cached_proc_cred(PROC_NULL);

	bzero((caddr_t)&info, sizeof(info));

	for (pass = 0; pass < 2; pass++) {
		ifnet_head_lock_shared();

		TAILQ_FOREACH(ifp, &ifnet_head, if_link) {
			if (error) {
				break;
			}
			if (w->w_arg && w->w_arg != ifp->if_index) {
				continue;
			}
			ifnet_lock_shared(ifp);
			/*
			 * Holding ifnet lock here prevents the link address
			 * from changing contents, so no need to hold the ifa
			 * lock.  The link address is always present; it's
			 * never freed.
			 */
			ifa = ifp->if_lladdr;
			info.rti_info[RTAX_IFP] = ifa->ifa_addr;
			len = rt_msg2(RTM_IFINFO, &info, NULL, NULL, &cred);
			if (pass == 0) {
				if (os_add_overflow(total_len, len, &total_len)) {
					ifnet_lock_done(ifp);
					error = ENOBUFS;
					break;
				}
			} else {
				struct if_msghdr *ifm;

				if (current_len + len > total_len) {
					ifnet_lock_done(ifp);
					error = ENOBUFS;
					break;
				}
				info.rti_info[RTAX_IFP] = ifa->ifa_addr;
				len = rt_msg2(RTM_IFINFO, &info,
				    (caddr_t)cp, NULL, &cred);
				info.rti_info[RTAX_IFP] = NULL;

				ifm = (struct if_msghdr *)(void *)cp;
				ifm->ifm_index = ifp->if_index;
				ifm->ifm_flags = (u_short)ifp->if_flags;
				if_data_internal_to_if_data(ifp, &ifp->if_data,
				    &ifm->ifm_data);
				ifm->ifm_addrs = info.rti_addrs;
				/*
				 * <rdar://problem/32940901>
				 * Round bytes only for non-platform
				 */
				if (!csproc_get_platform_binary(w->w_req->p)) {
					ALIGN_BYTES(ifm->ifm_data.ifi_ibytes);
					ALIGN_BYTES(ifm->ifm_data.ifi_obytes);
				}

				cp += len;
				VERIFY(IS_P2ALIGNED(cp, sizeof(u_int32_t)));
				current_len += len;
				VERIFY(current_len <= total_len);
			}
			while ((ifa = ifa->ifa_link.tqe_next) != NULL) {
				boolean_t is_clat46;

				IFA_LOCK(ifa);
				if (af && af != ifa->ifa_addr->sa_family) {
					IFA_UNLOCK(ifa);
					continue;
				}
				is_clat46 = is_clat46_address(ifa);
				if (is_clat46) {
					if (!include_clat46_valid) {
						include_clat46_valid = TRUE;
						include_clat46 =
						    should_include_clat46();
					}
					if (!include_clat46) {
						IFA_UNLOCK(ifa);
						continue;
					}
				}
				info.rti_info[RTAX_IFA] = ifa->ifa_addr;
				info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;
				info.rti_info[RTAX_BRD] = ifa->ifa_dstaddr;
				len = rt_msg2(RTM_NEWADDR, &info, NULL, NULL,
				    &cred);
				if (pass == 0) {
					if (os_add_overflow(total_len, len, &total_len)) {
						IFA_UNLOCK(ifa);
						error = ENOBUFS;
						break;
					}
				} else {
					struct ifa_msghdr *ifam;

					if (current_len + len > total_len) {
						IFA_UNLOCK(ifa);
						error = ENOBUFS;
						break;
					}
					len = rt_msg2(RTM_NEWADDR, &info,
					    (caddr_t)cp, NULL, &cred);

					ifam = (struct ifa_msghdr *)(void *)cp;
					ifam->ifam_index =
					    ifa->ifa_ifp->if_index;
					ifam->ifam_flags = ifa->ifa_flags;
					ifam->ifam_metric = ifa->ifa_metric;
					ifam->ifam_addrs = info.rti_addrs;

					cp += len;
					VERIFY(IS_P2ALIGNED(cp,
					    sizeof(u_int32_t)));
					current_len += len;
					VERIFY(current_len <= total_len);
				}
				IFA_UNLOCK(ifa);
			}
			ifnet_lock_done(ifp);
			info.rti_info[RTAX_IFA] = info.rti_info[RTAX_NETMASK] =
			    info.rti_info[RTAX_BRD] = NULL;
		}

		ifnet_head_done();

		if (error != 0) {
			if (error == ENOBUFS) {
				printf("%s: current_len (%lu) + len (%lu) > "
				    "total_len (%lu)\n", __func__, current_len,
				    len, total_len);
			}
			break;
		}

		if (pass == 0) {
			/* Better to return zero length buffer than ENOBUFS */
			if (total_len == 0) {
				total_len = 1;
			}
			total_len += total_len >> 3;
			total_buffer_len = total_len;
			total_buffer = (char *) kalloc_data(total_len, Z_ZERO | Z_WAITOK);
			if (total_buffer == NULL) {
				printf("%s: kalloc_data(%lu) failed\n", __func__,
				    total_len);
				error = ENOBUFS;
				break;
			}
			cp = total_buffer;
			VERIFY(IS_P2ALIGNED(cp, sizeof(u_int32_t)));
		} else {
			error = SYSCTL_OUT(w->w_req, total_buffer, current_len);
			if (error) {
				break;
			}
		}
	}

	if (total_buffer != NULL) {
		kfree_data(total_buffer, total_buffer_len);
	}

	return error;
}

static int
sysctl_iflist2(int af, struct walkarg *w)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct  rt_addrinfo info;
	int     error = 0;
	int     pass = 0;
	size_t  len = 0, total_len = 0, total_buffer_len = 0, current_len = 0;
	char    *total_buffer = NULL, *cp = NULL;
	kauth_cred_t cred __single;
	boolean_t include_clat46 = FALSE;
	boolean_t include_clat46_valid = FALSE;

	cred = current_cached_proc_cred(PROC_NULL);

	bzero((caddr_t)&info, sizeof(info));

	for (pass = 0; pass < 2; pass++) {
		struct ifmultiaddr *ifma;

		ifnet_head_lock_shared();

		TAILQ_FOREACH(ifp, &ifnet_head, if_link) {
			if (error) {
				break;
			}
			if (w->w_arg && w->w_arg != ifp->if_index) {
				continue;
			}
			ifnet_lock_shared(ifp);
			/*
			 * Holding ifnet lock here prevents the link address
			 * from changing contents, so no need to hold the ifa
			 * lock.  The link address is always present; it's
			 * never freed.
			 */
			ifa = ifp->if_lladdr;
			info.rti_info[RTAX_IFP] = ifa->ifa_addr;
			len = rt_msg2(RTM_IFINFO2, &info, NULL, NULL, &cred);
			if (pass == 0) {
				if (os_add_overflow(total_len, len, &total_len)) {
					ifnet_lock_done(ifp);
					error = ENOBUFS;
					break;
				}
			} else {
				struct if_msghdr2 *ifm;

				if (current_len + len > total_len) {
					ifnet_lock_done(ifp);
					error = ENOBUFS;
					break;
				}
				info.rti_info[RTAX_IFP] = ifa->ifa_addr;
				len = rt_msg2(RTM_IFINFO2, &info,
				    (caddr_t)cp, NULL, &cred);
				info.rti_info[RTAX_IFP] = NULL;

				ifm = (struct if_msghdr2 *)(void *)cp;
				ifm->ifm_addrs = info.rti_addrs;
				ifm->ifm_flags = (u_short)ifp->if_flags;
				ifm->ifm_index = ifp->if_index;
				ifm->ifm_snd_len = IFCQ_LEN(ifp->if_snd);
				ifm->ifm_snd_maxlen = IFCQ_MAXLEN(ifp->if_snd);
				ifm->ifm_snd_drops =
				    (int)ifp->if_snd->ifcq_dropcnt.packets;
				ifm->ifm_timer = ifp->if_timer;
				if_data_internal_to_if_data64(ifp,
				    &ifp->if_data, &ifm->ifm_data);
				/*
				 * <rdar://problem/32940901>
				 * Round bytes only for non-platform
				 */
				if (!csproc_get_platform_binary(w->w_req->p)) {
					ALIGN_BYTES(ifm->ifm_data.ifi_ibytes);
					ALIGN_BYTES(ifm->ifm_data.ifi_obytes);
				}

				cp += len;
				VERIFY(IS_P2ALIGNED(cp, sizeof(u_int32_t)));
				current_len += len;
				VERIFY(current_len <= total_len);
			}
			while ((ifa = ifa->ifa_link.tqe_next) != NULL) {
				boolean_t is_clat46;

				IFA_LOCK(ifa);
				if (af && af != ifa->ifa_addr->sa_family) {
					IFA_UNLOCK(ifa);
					continue;
				}
				is_clat46 = is_clat46_address(ifa);
				if (is_clat46) {
					if (!include_clat46_valid) {
						include_clat46_valid = TRUE;
						include_clat46 =
						    should_include_clat46();
					}
					if (!include_clat46) {
						IFA_UNLOCK(ifa);
						continue;
					}
				}
				info.rti_info[RTAX_IFA] = ifa->ifa_addr;
				info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;
				info.rti_info[RTAX_BRD] = ifa->ifa_dstaddr;
				len = rt_msg2(RTM_NEWADDR, &info, NULL, NULL,
				    &cred);
				if (pass == 0) {
					if (os_add_overflow(total_len, len, &total_len)) {
						IFA_UNLOCK(ifa);
						error = ENOBUFS;
						break;
					}
				} else {
					struct ifa_msghdr *ifam;

					if (current_len + len > total_len) {
						IFA_UNLOCK(ifa);
						error = ENOBUFS;
						break;
					}
					len = rt_msg2(RTM_NEWADDR, &info,
					    (caddr_t)cp, NULL, &cred);

					ifam = (struct ifa_msghdr *)(void *)cp;
					ifam->ifam_index =
					    ifa->ifa_ifp->if_index;
					ifam->ifam_flags = ifa->ifa_flags;
					ifam->ifam_metric = ifa->ifa_metric;
					ifam->ifam_addrs = info.rti_addrs;

					cp += len;
					VERIFY(IS_P2ALIGNED(cp,
					    sizeof(u_int32_t)));
					current_len += len;
					VERIFY(current_len <= total_len);
				}
				IFA_UNLOCK(ifa);
			}
			if (error) {
				ifnet_lock_done(ifp);
				break;
			}

			for (ifma = LIST_FIRST(&ifp->if_multiaddrs);
			    ifma != NULL; ifma = LIST_NEXT(ifma, ifma_link)) {
				struct ifaddr *ifa0;

				IFMA_LOCK(ifma);
				if (af && af != ifma->ifma_addr->sa_family) {
					IFMA_UNLOCK(ifma);
					continue;
				}
				bzero((caddr_t)&info, sizeof(info));
				info.rti_info[RTAX_IFA] = ifma->ifma_addr;
				/*
				 * Holding ifnet lock here prevents the link
				 * address from changing contents, so no need
				 * to hold the ifa0 lock.  The link address is
				 * always present; it's never freed.
				 */
				ifa0 = ifp->if_lladdr;
				info.rti_info[RTAX_IFP] = ifa0->ifa_addr;
				if (ifma->ifma_ll != NULL) {
					info.rti_info[RTAX_GATEWAY] =
					    ifma->ifma_ll->ifma_addr;
				}
				len = rt_msg2(RTM_NEWMADDR2, &info, NULL, NULL,
				    &cred);
				if (pass == 0) {
					total_len += len;
				} else {
					struct ifma_msghdr2 *ifmam;

					if (current_len + len > total_len) {
						IFMA_UNLOCK(ifma);
						error = ENOBUFS;
						break;
					}
					len = rt_msg2(RTM_NEWMADDR2, &info,
					    (caddr_t)cp, NULL, &cred);

					ifmam =
					    (struct ifma_msghdr2 *)(void *)cp;
					ifmam->ifmam_addrs = info.rti_addrs;
					ifmam->ifmam_flags = 0;
					ifmam->ifmam_index =
					    ifma->ifma_ifp->if_index;
					ifmam->ifmam_refcount =
					    ifma->ifma_reqcnt;

					cp += len;
					VERIFY(IS_P2ALIGNED(cp,
					    sizeof(u_int32_t)));
					current_len += len;
				}
				IFMA_UNLOCK(ifma);
			}
			ifnet_lock_done(ifp);
			info.rti_info[RTAX_IFA] = info.rti_info[RTAX_NETMASK] =
			    info.rti_info[RTAX_BRD] = NULL;
		}
		ifnet_head_done();

		if (error) {
			if (error == ENOBUFS) {
				printf("%s: current_len (%lu) + len (%lu) > "
				    "total_len (%lu)\n", __func__, current_len,
				    len, total_len);
			}
			break;
		}

		if (pass == 0) {
			/* Better to return zero length buffer than ENOBUFS */
			if (total_len == 0) {
				total_len = 1;
			}
			total_len += total_len >> 3;
			total_buffer_len = total_len;
			total_buffer = (char *) kalloc_data(total_len, Z_ZERO | Z_WAITOK);
			if (total_buffer == NULL) {
				printf("%s: kalloc_data(%lu) failed\n", __func__,
				    total_len);
				error = ENOBUFS;
				break;
			}
			cp = total_buffer;
			VERIFY(IS_P2ALIGNED(cp, sizeof(u_int32_t)));
		} else {
			error = SYSCTL_OUT(w->w_req, total_buffer, current_len);
			if (error) {
				break;
			}
		}
	}

	if (total_buffer != NULL) {
		kfree_data(total_buffer, total_buffer_len);
	}

	return error;
}


static int
sysctl_rtstat(struct sysctl_req *req)
{
	struct rtstat rtstat_compat = { 0 };

#define RTSTAT_COMPAT(_field) rtstat_compat._field = rtstat._field < SHRT_MAX ? (short)rtstat._field : SHRT_MAX
	RTSTAT_COMPAT(rts_badredirect);
	RTSTAT_COMPAT(rts_dynamic);
	RTSTAT_COMPAT(rts_newgateway);
	RTSTAT_COMPAT(rts_unreach);
	RTSTAT_COMPAT(rts_wildcard);
	RTSTAT_COMPAT(rts_badrtgwroute);
#undef RTSTAT_TO_COMPAT

	return SYSCTL_OUT(req, &rtstat_compat, sizeof(struct rtstat));
}

static int
sysctl_rtstat_64(struct sysctl_req *req)
{
	return SYSCTL_OUT(req, &rtstat, sizeof(struct rtstat_64));
}

static int
sysctl_rttrash(struct sysctl_req *req)
{
	return SYSCTL_OUT(req, &rttrash, sizeof(rttrash));
}

static int
sysctl_rtsock SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp)
	DECLARE_SYSCTL_HANDLER_ARG_ARRAY(int, 4, name, namelen);
	struct radix_node_head *rnh;
	int     i, error = EINVAL;
	u_char  af;
	struct  walkarg w;

	name++;
	namelen--;
	if (req->newptr) {
		return EPERM;
	}
	af = (u_char)name[0];
	Bzero(&w, sizeof(w));
	w.w_op = name[1];
	w.w_arg = name[2];
	w.w_req = req;

	switch (w.w_op) {
	case NET_RT_DUMP:
	case NET_RT_DUMP2:
	case NET_RT_FLAGS:
	case NET_RT_FLAGS_PRIV:
		lck_mtx_lock(rnh_lock);
		for (i = 1; i <= AF_MAX; i++) {
			if ((rnh = rt_tables[i]) && (af == 0 || af == i) &&
			    (error = rnh->rnh_walktree(rnh,
			    sysctl_dumpentry, &w))) {
				break;
			}
		}
		lck_mtx_unlock(rnh_lock);
		break;
	case NET_RT_DUMPX:
	case NET_RT_DUMPX_FLAGS:
		lck_mtx_lock(rnh_lock);
		for (i = 1; i <= AF_MAX; i++) {
			if ((rnh = rt_tables[i]) && (af == 0 || af == i) &&
			    (error = rnh->rnh_walktree(rnh,
			    sysctl_dumpentry_ext, &w))) {
				break;
			}
		}
		lck_mtx_unlock(rnh_lock);
		break;
	case NET_RT_IFLIST:
		error = sysctl_iflist(af, &w);
		break;
	case NET_RT_IFLIST2:
		error = sysctl_iflist2(af, &w);
		break;
	case NET_RT_STAT:
		error = sysctl_rtstat(req);
		break;
	case NET_RT_STAT_64:
		error = sysctl_rtstat_64(req);
		break;
	case NET_RT_TRASH:
		error = sysctl_rttrash(req);
		break;
	}
	if (w.w_tmem != NULL) {
		kfree_data_sized_by(w.w_tmem, w.w_tmemsize);
	}
	return error;
}

/*
 * Definitions of protocols supported in the ROUTE domain.
 */
static struct protosw routesw[] = {
	{
		.pr_type =              SOCK_RAW,
		.pr_protocol =          0,
		.pr_flags =             PR_ATOMIC | PR_ADDR,
		.pr_output =            route_output,
		.pr_ctlinput =          raw_ctlinput,
		.pr_usrreqs =           &route_usrreqs,
	}
};

static int route_proto_count = (sizeof(routesw) / sizeof(struct protosw));

struct domain routedomain_s = {
	.dom_family =           PF_ROUTE,
	.dom_name =             "route",
	.dom_init =             route_dinit,
};

static void
route_dinit(struct domain *dp)
{
	struct protosw *pr;
	int i;

	VERIFY(!(dp->dom_flags & DOM_INITIALIZED));
	VERIFY(routedomain == NULL);

	routedomain = dp;

	for (i = 0, pr = &routesw[0]; i < route_proto_count; i++, pr++) {
		net_add_proto(pr, dp, 1);
	}

	route_init();
}
