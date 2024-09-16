/*
 * Copyright (c) 1997-2021 Apple Inc. All rights reserved.
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
 *	@(#)ndrv.c	1.1 (MacOSX) 6/10/43
 * Justin Walker, 970604
 *   AF_NDRV support
 * 980130 - Cleanup, reorg, performance improvemements
 * 000816 - Removal of Y adapter cruft
 */

/*
 * PF_NDRV allows raw access to a specified network device, directly
 *  with a socket.  Expected use involves a socket option to request
 *  protocol packets.  This lets ndrv_output() call ifnet_output(), and
 *  lets DLIL find the proper recipient for incoming packets.
 *  The purpose here is for user-mode protocol implementation.
 * Note that "pure raw access" will still be accomplished with BPF.
 *
 * In addition to the former use, when combined with socket NKEs,
 * PF_NDRV permits a fairly flexible mechanism for implementing
 * strange protocol support.
 */
#include <mach/mach_types.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/proc.h>

#include <kern/queue.h>
#include <kern/assert.h>

#include <net/ndrv.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/ndrv_var.h>
#include <net/dlil.h>
#include <net/sockaddr_utils.h>

#if INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#endif
#include <netinet/if_ether.h>

SYSCTL_NODE(_net, OID_AUTO, ndrv,
    CTLFLAG_RW | CTLFLAG_LOCKED, 0, "");

static unsigned int ndrv_multi_max_count = NDRV_DMUX_MAX_DESCR;
SYSCTL_UINT(_net_ndrv, OID_AUTO, multi_max_count, CTLFLAG_RW | CTLFLAG_LOCKED,
    &ndrv_multi_max_count, 0, "Number of allowed multicast addresses per NRDV socket");

/*
 * The locking strategy relies on the PF_NRDRV domain mutex that protects both the
 * PCB list "ndrvl" and the sockets themselves
 */

static int ndrv_do_detach(struct ndrv_cb *);
static int ndrv_do_disconnect(struct ndrv_cb *);
static struct ndrv_cb *ndrv_find_inbound(struct ifnet *ifp, u_int32_t protocol_family);
static int ndrv_setspec(struct ndrv_cb *np, struct sockopt *sopt);
static int ndrv_delspec(struct ndrv_cb *);
static int ndrv_to_ifnet_demux(struct ndrv_demux_desc* ndrv, struct ifnet_demux_desc* ifdemux);
static void ndrv_handle_ifp_detach(u_int32_t family, short unit);
static int ndrv_do_add_multicast(struct ndrv_cb *np, struct sockopt *sopt);
static int ndrv_do_remove_multicast(struct ndrv_cb *np, struct sockopt *sopt);
static struct ndrv_multiaddr* ndrv_have_multicast(struct ndrv_cb *np, struct sockaddr* addr);
static void ndrv_remove_all_multicast(struct ndrv_cb *np);
static void ndrv_dominit(struct domain *);

u_int32_t  ndrv_sendspace = NDRVSNDQ;
u_int32_t  ndrv_recvspace = NDRVRCVQ;
TAILQ_HEAD(, ndrv_cb)   ndrvl = TAILQ_HEAD_INITIALIZER(ndrvl);

uint32_t ndrv_pcbcount = 0;
SYSCTL_UINT(_net_ndrv, OID_AUTO, pcbcount, CTLFLAG_RD | CTLFLAG_LOCKED,
    &ndrv_pcbcount, 0, "Number of NRDV sockets");

static struct domain *ndrvdomain = NULL;
extern struct domain ndrvdomain_s;

#define NDRV_PROTODEMUX_COUNT   10

/*
 * Verify these values match.
 * To keep clients from including dlil.h, we define
 * these values independently in ndrv.h. They must
 * match or a conversion function must be written.
 */
#if NDRV_DEMUXTYPE_ETHERTYPE != DLIL_DESC_ETYPE2
#error NDRV_DEMUXTYPE_ETHERTYPE must match DLIL_DESC_ETYPE2
#endif
#if NDRV_DEMUXTYPE_SAP != DLIL_DESC_SAP
#error NDRV_DEMUXTYPE_SAP must match DLIL_DESC_SAP
#endif
#if NDRV_DEMUXTYPE_SNAP != DLIL_DESC_SNAP
#error NDRV_DEMUXTYPE_SNAP must match DLIL_DESC_SNAP
#endif

/*
 * Protocol output - Called to output a raw network packet directly
 *  to the driver.
 */
static int
ndrv_output(struct mbuf *m, struct socket *so)
{
	struct ndrv_cb *np = sotondrvcb(so);
	struct ifnet *ifp = np->nd_if;
	int result = 0;

#if NDRV_DEBUG
	printf("NDRV output: %x, %x, %x\n", m, so, np);
#endif

	/*
	 * No header is a format error
	 */
	if ((m->m_flags & M_PKTHDR) == 0) {
		return EINVAL;
	}

	so_update_tx_data_stats(so, 1, m->m_pkthdr.len);

	/* Unlock before calling ifnet_output */
	socket_unlock(so, 0);

	/*
	 * Call DLIL if we can. DLIL is much safer than calling the
	 * ifp directly.
	 */
	result = ifnet_output_raw(ifp, np->nd_proto_family, m);

	socket_lock(so, 0);

	return result;
}

/* Our input routine called from DLIL */
static errno_t
ndrv_input(
	ifnet_t                         ifp,
	protocol_family_t       proto_family,
	mbuf_t                          m,
	char                            *orig_frame_header)
{
	struct socket *so;
	struct sockaddr_dl ndrvsrc = {};
	struct ndrv_cb *np;
	char *frame_header = __unsafe_forge_bidi_indexable(char *,
	    orig_frame_header,
	    ifnet_hdrlen(ifp));
	int error = 0;

	ndrvsrc.sdl_len = sizeof(struct sockaddr_dl);
	ndrvsrc.sdl_family = AF_NDRV;
	ndrvsrc.sdl_index = 0;

	/* move packet from if queue to socket */
	/* Should be media-independent */
	ndrvsrc.sdl_type = IFT_ETHER;
	ndrvsrc.sdl_nlen = 0;
	ndrvsrc.sdl_alen = 6;
	ndrvsrc.sdl_slen = 0;
	bcopy(frame_header, &ndrvsrc.sdl_data, 6);

	/* prepend the frame header */
	m = m_prepend(m, ifnet_hdrlen(ifp), M_NOWAIT);
	if (m == NULL) {
		return EJUSTRETURN;
	}
	bcopy(frame_header, m_mtod_current(m), ifnet_hdrlen(ifp));

	/*
	 * We need to take the domain mutex before the list RW lock
	 */
	LCK_MTX_ASSERT(ndrvdomain->dom_mtx, LCK_MTX_ASSERT_NOTOWNED);
	lck_mtx_lock(ndrvdomain->dom_mtx);

	np = ndrv_find_inbound(ifp, proto_family);
	if (np == NULL) {
		lck_mtx_unlock(ndrvdomain->dom_mtx);
		return ENOENT;
	}

	so = np->nd_socket;

	if (sbappendaddr(&(so->so_rcv), (struct sockaddr *)&ndrvsrc,
	    m, NULL, &error) != 0) {
		sorwakeup(so);
	}

	lck_mtx_unlock(ndrvdomain->dom_mtx);

	return 0; /* radar 4030377 - always return 0 */
}

/*
 * Allocate an ndrv control block and some buffer space for the socket
 */
static int
ndrv_attach(struct socket *so, int proto, __unused struct proc *p)
{
	int error;
	struct ndrv_cb *np = sotondrvcb(so);

	if ((so->so_state & SS_PRIV) == 0) {
		return EPERM;
	}

#if NDRV_DEBUG
	printf("NDRV attach: %x, %x, %x\n", so, proto, np);
#endif

	if ((error = soreserve(so, ndrv_sendspace, ndrv_recvspace))) {
		return error;
	}

	np = kalloc_type(struct ndrv_cb, Z_WAITOK | Z_ZERO | Z_NOFAIL);
	so->so_pcb = (caddr_t)np;
#if NDRV_DEBUG
	printf("NDRV attach: %x, %x, %x\n", so, proto, np);
#endif
	TAILQ_INIT(&np->nd_dlist);
	np->nd_signature = NDRV_SIGNATURE;
	np->nd_socket = so;
	np->nd_proto.sp_family = (uint16_t)SOCK_DOM(so);
	np->nd_proto.sp_protocol = (uint16_t)proto;
	np->nd_if = NULL;
	np->nd_proto_family = 0;
	np->nd_family = 0;
	np->nd_unit = 0;

	/*
	 * Use the domain mutex to protect the list
	 */
	LCK_MTX_ASSERT(ndrvdomain->dom_mtx, LCK_MTX_ASSERT_NOTOWNED);
	lck_mtx_lock(ndrvdomain->dom_mtx);

	TAILQ_INSERT_TAIL(&ndrvl, np, nd_next);
	ndrv_pcbcount++;

	lck_mtx_unlock(ndrvdomain->dom_mtx);

	return 0;
}

/*
 * Destroy state just before socket deallocation.
 * Flush data or not depending on the options.
 */

static int
ndrv_detach(struct socket *so)
{
	struct ndrv_cb *np = sotondrvcb(so);

	if (np == 0) {
		return EINVAL;
	}
	return ndrv_do_detach(np);
}


/*
 * If a socket isn't bound to a single address,
 * the ndrv input routine will hand it anything
 * within that protocol family (assuming there's
 * nothing else around it should go to).
 *
 * Don't expect this to be used.
 */

static int
ndrv_connect(struct socket *so, struct sockaddr *nam, __unused struct proc *p)
{
	struct ndrv_cb *np = sotondrvcb(so);

	if (np == 0) {
		return EINVAL;
	}

	if (np->nd_faddr) {
		return EISCONN;
	}

	if (nam->sa_len < sizeof(struct sockaddr_ndrv)) {
		return EINVAL;
	}

	/* Allocate memory to store the remote address */
	np->nd_faddr = kalloc_type(struct sockaddr_ndrv, Z_WAITOK | Z_NOFAIL | Z_ZERO);

	SOCKADDR_COPY(nam, np->nd_faddr,
	    MIN(sizeof(struct sockaddr_ndrv), nam->sa_len));
	np->nd_faddr->snd_len = sizeof(struct sockaddr_ndrv);
	soisconnected(so);
	return 0;
}

static void
ndrv_event(struct ifnet *ifp, __unused protocol_family_t protocol,
    const struct kev_msg *event)
{
	if (event->vendor_code == KEV_VENDOR_APPLE &&
	    event->kev_class == KEV_NETWORK_CLASS &&
	    event->kev_subclass == KEV_DL_SUBCLASS &&
	    event->event_code == KEV_DL_IF_DETACHING) {
		LCK_MTX_ASSERT(ndrvdomain->dom_mtx, LCK_MTX_ASSERT_NOTOWNED);
		lck_mtx_lock(ndrvdomain->dom_mtx);
		ndrv_handle_ifp_detach(ifnet_family(ifp), ifp->if_unit);
		lck_mtx_unlock(ndrvdomain->dom_mtx);
	}
}

/*
 * This is the "driver open" hook - we 'bind' to the
 *  named driver.
 * Here's where we latch onto the driver.
 */
static int
ndrv_bind(struct socket *so, struct sockaddr *nam, __unused struct proc *p)
{
	struct sockaddr_ndrv *sa = (struct sockaddr_ndrv *) nam;
	const char *dname;
	struct ndrv_cb *np;
	struct ifnet *ifp;
	int result;

	if (TAILQ_EMPTY(&ifnet_head)) {
		return EADDRNOTAVAIL;        /* Quick sanity check */
	}
	np = sotondrvcb(so);
	if (np == 0) {
		return EINVAL;
	}

	if (np->nd_laddr) {
		return EINVAL;                  /* XXX */
	}
	/* I think we just latch onto a copy here; the caller frees */
	np->nd_laddr = kalloc_type(struct sockaddr_ndrv, Z_WAITOK | Z_NOFAIL | Z_ZERO);
	SOCKADDR_COPY(sa, np->nd_laddr,
	    MIN(sizeof(struct sockaddr_ndrv), sa->snd_len));
	np->nd_laddr->snd_len = sizeof(struct sockaddr_ndrv);
	dname = (const char *) sa->snd_name;
	if (*dname == '\0') {
		return EINVAL;
	}
#if NDRV_DEBUG
	printf("NDRV bind: %x, %x, %s\n", so, np, dname);
#endif
	/* Track down the driver and its ifnet structure.
	 * There's no internal call for this so we have to dup the code
	 *  in if.c/ifconf()
	 */
	ifnet_head_lock_shared();
	TAILQ_FOREACH(ifp, &ifnet_head, if_link) {
		if (strlcmp(dname, ifp->if_xname, IFNAMSIZ) == 0) {
			break;
		}
	}
	ifnet_head_done();

	if (ifp == NULL) {
		return EADDRNOTAVAIL;
	}

	// PPP doesn't support PF_NDRV.
	if (ifnet_family(ifp) != APPLE_IF_FAM_PPP) {
		/* NDRV on this interface */
		struct ifnet_attach_proto_param ndrv_proto;
		result = 0;
		bzero(&ndrv_proto, sizeof(ndrv_proto));
		ndrv_proto.event = ndrv_event;

		/* We aren't worried about double attaching, that should just return an error */
		socket_unlock(so, 0);
		result = ifnet_attach_protocol(ifp, PF_NDRV, &ndrv_proto);
		socket_lock(so, 0);
		if (result && result != EEXIST) {
			return result;
		}
		np->nd_proto_family = PF_NDRV;
	} else {
		np->nd_proto_family = 0;
	}

	np->nd_if = ifp;
	np->nd_family = ifnet_family(ifp);
	np->nd_unit = ifp->if_unit;

	return 0;
}

static int
ndrv_disconnect(struct socket *so)
{
	struct ndrv_cb *np = sotondrvcb(so);

	if (np == 0) {
		return EINVAL;
	}

	if (np->nd_faddr == 0) {
		return ENOTCONN;
	}

	ndrv_do_disconnect(np);
	return 0;
}

/*
 * Mark the connection as being incapable of further input.
 */
static int
ndrv_shutdown(struct socket *so)
{
	LCK_MTX_ASSERT(ndrvdomain->dom_mtx, LCK_MTX_ASSERT_OWNED);
	socantsendmore(so);
	return 0;
}

/*
 * Ship a packet out.  The ndrv output will pass it
 *  to the appropriate driver.  The really tricky part
 *  is the destination address...
 */
static int
ndrv_send(struct socket *so, __unused int flags, struct mbuf *m,
    __unused struct sockaddr *addr, struct mbuf *control,
    __unused struct proc *p)
{
	int error;

	if (control != NULL) {
		m_freem(control);
		return EOPNOTSUPP;
	}

	error = ndrv_output(m, so);
	m = NULL;
	return error;
}


static int
ndrv_abort(struct socket *so)
{
	struct ndrv_cb *np = sotondrvcb(so);

	if (np == 0) {
		return EINVAL;
	}

	ndrv_do_disconnect(np);
	return 0;
}

static int
ndrv_sockaddr(struct socket *so, struct sockaddr **nam)
{
	struct ndrv_cb *np = sotondrvcb(so);
	unsigned int len;

	if (np == 0) {
		return EINVAL;
	}

	if (np->nd_laddr == 0) {
		return EINVAL;
	}

	len = np->nd_laddr->snd_len;
	*nam = (struct sockaddr *)alloc_sockaddr(len,
	    Z_WAITOK | Z_NOFAIL);

	SOCKADDR_COPY(np->nd_laddr, *nam, len);
	return 0;
}


static int
ndrv_peeraddr(struct socket *so, struct sockaddr **nam)
{
	struct ndrv_cb *np = sotondrvcb(so);
	unsigned int len;

	if (np == 0) {
		return EINVAL;
	}

	if (np->nd_faddr == 0) {
		return ENOTCONN;
	}

	len = np->nd_faddr->snd_len;
	*nam = (struct sockaddr *)alloc_sockaddr(len,
	    Z_WAITOK | Z_NOFAIL);

	SOCKADDR_COPY(np->nd_faddr, *nam, len);
	return 0;
}


/* Control output */

static int
ndrv_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct ndrv_cb *np = sotondrvcb(so);
	int error = 0;

	switch (sopt->sopt_name) {
	case NDRV_DELDMXSPEC: /* Delete current spec */
		/* Verify no parameter was passed */
		if (sopt->sopt_val != 0 || sopt->sopt_valsize != 0) {
			/*
			 * We don't support deleting a specific demux, it's
			 * all or nothing.
			 */
			return EINVAL;
		}
		error = ndrv_delspec(np);
		break;
	case NDRV_SETDMXSPEC: /* Set protocol spec */
		error = ndrv_setspec(np, sopt);
		break;
	case NDRV_ADDMULTICAST:
		error = ndrv_do_add_multicast(np, sopt);
		break;
	case NDRV_DELMULTICAST:
		error = ndrv_do_remove_multicast(np, sopt);
		break;
	default:
		error = ENOTSUP;
	}
#ifdef NDRV_DEBUG
	log(LOG_WARNING, "NDRV CTLOUT: %x returns %d\n", sopt->sopt_name,
	    error);
#endif
	return error;
}

static int
ndrv_do_detach(struct ndrv_cb *np)
{
	struct ndrv_cb*     cur_np = NULL;
	struct socket *so = np->nd_socket;
	int error = 0;
	struct ifnet * ifp;

#if NDRV_DEBUG
	printf("NDRV detach: %x, %x\n", so, np);
#endif
	ndrv_remove_all_multicast(np);

	/* Remove from the linked list of control blocks */
	LCK_MTX_ASSERT(ndrvdomain->dom_mtx, LCK_MTX_ASSERT_OWNED);
	TAILQ_REMOVE(&ndrvl, np, nd_next);
	ndrv_pcbcount--;

	ifp = np->nd_if;
	if (ifp != NULL) {
		u_int32_t proto_family = np->nd_proto_family;

		if (proto_family != PF_NDRV && proto_family != 0) {
			socket_unlock(so, 0);
			ifnet_detach_protocol(ifp, proto_family);
			socket_lock(so, 0);
		}

		/* Check if this is the last socket attached to this interface */
		LCK_MTX_ASSERT(ndrvdomain->dom_mtx, LCK_MTX_ASSERT_OWNED);
		TAILQ_FOREACH(cur_np, &ndrvl, nd_next) {
			if (cur_np->nd_family == np->nd_family &&
			    cur_np->nd_unit == np->nd_unit) {
				break;
			}
		}

		/* If there are no other interfaces, detach PF_NDRV from the interface */
		if (cur_np == NULL) {
			socket_unlock(so, 0);
			ifnet_detach_protocol(ifp, PF_NDRV);
			socket_lock(so, 0);
		}
	}
	if (np->nd_laddr != NULL) {
		kfree_type(struct sockaddr_ndrv, np->nd_laddr);
	}
	kfree_type(struct ndrv_cb, np);
	so->so_pcb = 0;
	so->so_flags |= SOF_PCBCLEARING;
	sofree(so);
	return error;
}

static int
ndrv_do_disconnect(struct ndrv_cb *np)
{
	struct socket * so = np->nd_socket;
#if NDRV_DEBUG
	printf("NDRV disconnect: %x\n", np);
#endif
	if (np->nd_faddr) {
		kfree_type(struct sockaddr_ndrv, np->nd_faddr);
	}
	/*
	 * A multipath subflow socket would have its SS_NOFDREF set by default,
	 * so check for SOF_MP_SUBFLOW socket flag before detaching the PCB;
	 * when the socket is closed for real, SOF_MP_SUBFLOW would be cleared.
	 */
	if (!(so->so_flags & SOF_MP_SUBFLOW) && (so->so_state & SS_NOFDREF)) {
		ndrv_do_detach(np);
	}
	soisdisconnected(so);
	return 0;
}

#if 0
//### Not used
/*
 * When closing, dump any enqueued mbufs.
 */
void
ndrv_flushq(struct ifqueue *q)
{
	struct mbuf *m;
	for (;;) {
		IF_DEQUEUE(q, m);
		if (m == NULL) {
			break;
		}
		IF_DROP(q);
		if (m) {
			m_freem(m);
		}
	}
}
#endif

int
ndrv_setspec(struct ndrv_cb *np, struct sockopt *sopt)
{
	struct ifnet_attach_proto_param proto_param;
	struct ndrv_protocol_desc_kernel ndrvSpec;
	struct ndrv_demux_desc*         ndrvDemux = NULL;
	size_t                          ndrvDemuxSize = 0;
	int                             error = 0;
	struct socket *                 so = np->nd_socket;
	user_addr_t                     user_addr;
	uint32_t                        demux_count = 0;

	/* Sanity checking */
	if (np->nd_proto_family != PF_NDRV) {
		return EBUSY;
	}
	if (np->nd_if == NULL) {
		return EINVAL;
	}

	/* Copy the ndrvSpec */
	if (proc_is64bit(sopt->sopt_p)) {
		struct ndrv_protocol_desc64     ndrvSpec64;

		if (sopt->sopt_valsize != sizeof(ndrvSpec64)) {
			return EINVAL;
		}

		error = sooptcopyin(sopt, &ndrvSpec64, sizeof(ndrvSpec64), sizeof(ndrvSpec64));
		if (error != 0) {
			return error;
		}

		ndrvSpec.version         = ndrvSpec64.version;
		ndrvSpec.protocol_family = ndrvSpec64.protocol_family;
		demux_count              = ndrvSpec64.demux_count;

		user_addr = CAST_USER_ADDR_T(ndrvSpec64.demux_list);
	} else {
		struct ndrv_protocol_desc32     ndrvSpec32;

		if (sopt->sopt_valsize != sizeof(ndrvSpec32)) {
			return EINVAL;
		}

		error = sooptcopyin(sopt, &ndrvSpec32, sizeof(ndrvSpec32), sizeof(ndrvSpec32));
		if (error != 0) {
			return error;
		}

		ndrvSpec.version         = ndrvSpec32.version;
		ndrvSpec.protocol_family = ndrvSpec32.protocol_family;
		demux_count              = ndrvSpec32.demux_count;

		user_addr = CAST_USER_ADDR_T(ndrvSpec32.demux_list);
	}

	/*
	 * Do not allow PF_NDRV as it's non-sensical and most importantly because
	 * we use PF_NDRV to see if the protocol family has already been set
	 */
	if (ndrvSpec.protocol_family == PF_NDRV) {
		return EINVAL;
	}

	/* Verify the parameter */
	if (ndrvSpec.version > NDRV_PROTOCOL_DESC_VERS) {
		return ENOTSUP; // version is too new!
	} else if (ndrvSpec.version < 1) {
		return EINVAL; // version is not valid
	} else if (demux_count > NDRV_PROTODEMUX_COUNT || demux_count == 0) {
		return EINVAL; // demux_count is not valid
	}
	bzero(&proto_param, sizeof(proto_param));

	/* Allocate storage for demux array */
	ndrvDemuxSize = demux_count * sizeof(struct ndrv_demux_desc);
	ndrvDemux = (struct ndrv_demux_desc*) kalloc_data(ndrvDemuxSize, Z_WAITOK);
	if (ndrvDemux == NULL) {
		return ENOMEM;
	}

	/* Allocate enough ifnet_demux_descs */
	struct ifnet_demux_desc *demux_desc = kalloc_type(struct ifnet_demux_desc,
	    demux_count, Z_WAITOK | Z_ZERO);
	if (demux_desc == NULL) {
		error = ENOMEM;
	} else {
		proto_param.demux_array = demux_desc;
		proto_param.demux_count = demux_count;
	}

	if (error == 0) {
		/* Copy the ndrv demux array from userland */
		error = copyin(user_addr, ndrvDemux,
		    demux_count * sizeof(struct ndrv_demux_desc));
		ndrvSpec.demux_list = ndrvDemux;
		ndrvSpec.demux_count = demux_count;
	}

	if (error == 0) {
		/* At this point, we've at least got enough bytes to start looking around */
		u_int32_t       demuxOn = 0;

		proto_param.input = ndrv_input;
		proto_param.event = ndrv_event;

		for (demuxOn = 0; demuxOn < ndrvSpec.demux_count; demuxOn++) {
			/* Convert an ndrv_demux_desc to a ifnet_demux_desc */
			error = ndrv_to_ifnet_demux(&ndrvSpec.demux_list[demuxOn],
			    &proto_param.demux_array[demuxOn]);
			if (error) {
				break;
			}
		}
	}

	if (error == 0) {
		/*
		 * Set the protocol family to prevent other threads from
		 * attaching a protocol while the socket is unlocked
		 */
		np->nd_proto_family = ndrvSpec.protocol_family;
		socket_unlock(so, 0);
		error = ifnet_attach_protocol(np->nd_if, ndrvSpec.protocol_family,
		    &proto_param);
		socket_lock(so, 0);
		/*
		 * Upon failure, indicate that no protocol is attached
		 */
		if (error != 0) {
			np->nd_proto_family = PF_NDRV;
		}
	}

	/* Free any memory we've allocated */
	if (proto_param.demux_array != NULL) {
		kfree_type_counted_by(struct ifnet_demux_desc,
		    proto_param.demux_count,
		    proto_param.demux_array);
	}
	if (ndrvDemux) {
		ndrvSpec.demux_list = NULL;
		ndrvSpec.demux_count = 0;
		kfree_data(ndrvDemux, ndrvDemuxSize);
	}

	return error;
}


int
ndrv_to_ifnet_demux(struct ndrv_demux_desc* ndrv, struct ifnet_demux_desc* ifdemux)
{
	bzero(ifdemux, sizeof(*ifdemux));

	if (ndrv->type < DLIL_DESC_ETYPE2) {
		/* using old "type", not supported */
		return ENOTSUP;
	}

	if (ndrv->length > 28) {
		return EINVAL;
	}

	ifdemux->type = ndrv->type;
	ifdemux->data = ndrv->data.other;
	ifdemux->datalen = ndrv->length;

	return 0;
}

int
ndrv_delspec(struct ndrv_cb *np)
{
	int result = 0;

	if (np->nd_proto_family == PF_NDRV ||
	    np->nd_proto_family == 0) {
		return EINVAL;
	}

	/* Detach the protocol */
	result = ifnet_detach_protocol(np->nd_if, np->nd_proto_family);
	np->nd_proto_family = PF_NDRV;

	return result;
}

struct ndrv_cb *
ndrv_find_inbound(struct ifnet *ifp, u_int32_t protocol)
{
	struct ndrv_cb* np;

	LCK_MTX_ASSERT(ndrvdomain->dom_mtx, LCK_MTX_ASSERT_OWNED);

	if (protocol == PF_NDRV) {
		return NULL;
	}

	TAILQ_FOREACH(np, &ndrvl, nd_next) {
		if (np->nd_proto_family == protocol &&
		    np->nd_if == ifp) {
			return np;
		}
	}

	return NULL;
}

static void
ndrv_handle_ifp_detach(u_int32_t family, short unit)
{
	struct ndrv_cb* np;
	struct ifnet        *ifp = NULL;
	struct socket *so;

	/* Find all sockets using this interface. */
	TAILQ_FOREACH(np, &ndrvl, nd_next) {
		if (np->nd_family == family &&
		    np->nd_unit == unit) {
			/* This cb is using the detaching interface, but not for long. */
			/* Let the protocol go */
			ifp = np->nd_if;
			if (np->nd_proto_family != 0) {
				ndrv_delspec(np);
			}

			/* Delete the multicasts first */
			ndrv_remove_all_multicast(np);

			/* Disavow all knowledge of the ifp */
			np->nd_if = NULL;
			np->nd_unit = 0;
			np->nd_family = 0;

			so = np->nd_socket;
			/* Make sure sending returns an error */
			LCK_MTX_ASSERT(ndrvdomain->dom_mtx, LCK_MTX_ASSERT_OWNED);
			socantsendmore(so);
			socantrcvmore(so);
		}
	}

	/* Unregister our protocol */
	if (ifp) {
		ifnet_detach_protocol(ifp, PF_NDRV);
	}
}

static void
ndrv_multiaddr_free(struct ndrv_multiaddr *ndrv_multi)
{
	kfree_data(ndrv_multi->addr, ndrv_multi->addr->sa_len);
	kfree_type(struct ndrv_multiaddr, ndrv_multi);
}

static int
ndrv_do_add_multicast(struct ndrv_cb *np, struct sockopt *sopt)
{
	struct ndrv_multiaddr *ndrv_multi = NULL;
	struct sockaddr       *addr = NULL;
	int                    result;

	if (sopt->sopt_val == 0 || sopt->sopt_valsize < 2 ||
	    sopt->sopt_level != SOL_NDRVPROTO || sopt->sopt_valsize > SOCK_MAXADDRLEN) {
		return EINVAL;
	}
	if (np->nd_if == NULL) {
		return ENXIO;
	}
	if (!(np->nd_dlist_cnt < ndrv_multi_max_count)) {
		return EPERM;
	}

	// Copy in the address
	addr = kalloc_data(sopt->sopt_valsize, Z_WAITOK_ZERO_NOFAIL);
	result = copyin(sopt->sopt_val, addr, sopt->sopt_valsize);
	if (result == 0) {
		ndrv_multi = kalloc_type(struct ndrv_multiaddr, Z_WAITOK_ZERO_NOFAIL);
		ndrv_multi->addr = addr;
		addr = NULL; // don't use addr again
	}

	// Validate the sockaddr
	if (result == 0 && sopt->sopt_valsize != ndrv_multi->addr->sa_len) {
		result = EINVAL;
	}

	if (result == 0 && ndrv_have_multicast(np, ndrv_multi->addr)) {
		result = EEXIST;
	}

	if (result == 0) {
		// Try adding the multicast
		result = ifnet_add_multicast(np->nd_if, ndrv_multi->addr,
		    &ndrv_multi->ifma);
	}

	if (result == 0) {
		// Add to our linked list
		ndrv_multi->next = np->nd_multiaddrs;
		np->nd_multiaddrs = ndrv_multi;
		np->nd_dlist_cnt++;
	} else {
		// Free up the memory, something went wrong
		if (ndrv_multi != NULL) {
			ndrv_multiaddr_free(ndrv_multi);
		} else if (addr != NULL) {
			kfree_data(addr, sopt->sopt_valsize);
		}
	}

	return result;
}

static void
ndrv_cb_remove_multiaddr(struct ndrv_cb *np, struct ndrv_multiaddr *ndrv_entry)
{
	struct ndrv_multiaddr   *cur = np->nd_multiaddrs;
	bool                    removed = false;

	if (cur == ndrv_entry) {
		/* we were the head */
		np->nd_multiaddrs = cur->next;
		removed = true;
	} else {
		/* find our entry */
		struct ndrv_multiaddr  *cur_next = NULL;

		for (; cur != NULL; cur = cur_next) {
			cur_next = cur->next;
			if (cur_next == ndrv_entry) {
				cur->next = cur_next->next;
				removed = true;
				break;
			}
		}
	}
	ASSERT(removed);
}

static int
ndrv_do_remove_multicast(struct ndrv_cb *np, struct sockopt *sopt)
{
	struct sockaddr*            multi_addr;
	struct ndrv_multiaddr*      ndrv_entry = NULL;
	int                                 result;

	if (sopt->sopt_val == 0 || sopt->sopt_valsize < 2 ||
	    sopt->sopt_valsize > SOCK_MAXADDRLEN ||
	    sopt->sopt_level != SOL_NDRVPROTO) {
		return EINVAL;
	}
	if (np->nd_if == NULL || np->nd_dlist_cnt == 0) {
		return ENXIO;
	}

	// Allocate storage
	multi_addr = (struct sockaddr*) kalloc_data(sopt->sopt_valsize, Z_WAITOK);
	if (multi_addr == NULL) {
		return ENOMEM;
	}

	// Copy in the address
	result = copyin(sopt->sopt_val, multi_addr, sopt->sopt_valsize);

	// Validate the sockaddr
	if (result == 0 && sopt->sopt_valsize != multi_addr->sa_len) {
		result = EINVAL;
	}

	if (result == 0) {
		/* Find the old entry */
		ndrv_entry = ndrv_have_multicast(np, multi_addr);

		if (ndrv_entry == NULL) {
			result = ENOENT;
		}
	}

	if (result == 0) {
		// Try deleting the multicast
		result = ifnet_remove_multicast(ndrv_entry->ifma);
	}

	if (result == 0) {
		// Remove from our linked list
		ifmaddr_release(ndrv_entry->ifma);

		ndrv_cb_remove_multiaddr(np, ndrv_entry);
		np->nd_dlist_cnt--;

		ndrv_multiaddr_free(ndrv_entry);
	}
	kfree_data(multi_addr, sopt->sopt_valsize);

	return result;
}

static struct ndrv_multiaddr*
ndrv_have_multicast(struct ndrv_cb *np, struct sockaddr* inAddr)
{
	struct ndrv_multiaddr*      cur;
	for (cur = np->nd_multiaddrs; cur != NULL; cur = cur->next) {
		if ((inAddr->sa_len == cur->addr->sa_len) &&
		    (SOCKADDR_CMP(cur->addr, inAddr, inAddr->sa_len) == 0)) {
			// Found a match
			return cur;
		}
	}

	return NULL;
}

static void
ndrv_remove_all_multicast(struct ndrv_cb* np)
{
	struct ndrv_multiaddr*      cur;

	if (np->nd_if != NULL) {
		while (np->nd_multiaddrs != NULL) {
			cur = np->nd_multiaddrs;
			np->nd_multiaddrs = cur->next;

			ifnet_remove_multicast(cur->ifma);
			ifmaddr_release(cur->ifma);
			ndrv_multiaddr_free(cur);
		}
	}
}

static struct pr_usrreqs ndrv_usrreqs = {
	.pru_abort =            ndrv_abort,
	.pru_attach =           ndrv_attach,
	.pru_bind =             ndrv_bind,
	.pru_connect =          ndrv_connect,
	.pru_detach =           ndrv_detach,
	.pru_disconnect =       ndrv_disconnect,
	.pru_peeraddr =         ndrv_peeraddr,
	.pru_send =             ndrv_send,
	.pru_shutdown =         ndrv_shutdown,
	.pru_sockaddr =         ndrv_sockaddr,
	.pru_sosend =           sosend,
	.pru_soreceive =        soreceive,
};

static struct protosw ndrvsw[] = {
	{
		.pr_type =              SOCK_RAW,
		.pr_protocol =          NDRVPROTO_NDRV,
		.pr_flags =             PR_ATOMIC | PR_ADDR,
		.pr_output =            ndrv_output,
		.pr_ctloutput =         ndrv_ctloutput,
		.pr_usrreqs =           &ndrv_usrreqs,
	}
};

static int ndrv_proto_count = (sizeof(ndrvsw) / sizeof(struct protosw));

struct domain ndrvdomain_s = {
	.dom_family =           PF_NDRV,
	.dom_name =             "NetDriver",
	.dom_init =             ndrv_dominit,
};

static void
ndrv_dominit(struct domain *dp)
{
	struct protosw *pr;
	int i;

	VERIFY(!(dp->dom_flags & DOM_INITIALIZED));
	VERIFY(ndrvdomain == NULL);

	ndrvdomain = dp;

	for (i = 0, pr = &ndrvsw[0]; i < ndrv_proto_count; i++, pr++) {
		net_add_proto(pr, dp, 1);
	}
}
