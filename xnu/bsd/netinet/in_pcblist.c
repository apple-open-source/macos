/*
 * Copyright (c) 2010-2021 Apple Inc. All rights reserved.
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
 * Copyright (c) 1982, 1986, 1990, 1993
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
 */

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/filedesc.h>
#include <sys/file_internal.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/dtrace.h>
#include <sys/kauth.h>

#include <net/route.h>
#include <net/if_var.h>
#include <net/if_ports_used.h>
#include <net/ntstat.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet6/in6_var.h>

#include <net/sockaddr_utils.h>

#include <os/log.h>

#ifndef ROUNDUP64
#define ROUNDUP64(x) P2ROUNDUP((x), sizeof (u_int64_t))
#endif

#ifndef ADVANCE64
#define ADVANCE64(p, n) (void*)((char *)(p) + ROUNDUP64(n))
#endif

static void inpcb_to_xinpcb_n(struct inpcb *, struct xinpcb_n *);
static void tcpcb_to_xtcpcb_n(struct tcpcb *, struct xtcpcb_n *);
void shutdown_sockets_on_interface(struct ifnet *ifp);


__private_extern__ void
sotoxsocket_n(struct socket *so, struct xsocket_n *xso)
{
	xso->xso_len = sizeof(struct xsocket_n);
	xso->xso_kind = XSO_SOCKET;

	if (so == NULL) {
		return;
	}

	xso->xso_so = (uint64_t)VM_KERNEL_ADDRHASH(so);
	xso->so_type = so->so_type;
	xso->so_options = so->so_options;
	xso->so_linger = so->so_linger;
	xso->so_state = so->so_state;
	xso->so_pcb = (uint64_t)VM_KERNEL_ADDRHASH(so->so_pcb);
	if (so->so_proto) {
		xso->xso_protocol = SOCK_PROTO(so);
		xso->xso_family = SOCK_DOM(so);
	} else {
		xso->xso_protocol = xso->xso_family = 0;
	}
	xso->so_qlen = so->so_qlen;
	xso->so_incqlen = so->so_incqlen;
	xso->so_qlimit = so->so_qlimit;
	xso->so_timeo = so->so_timeo;
	xso->so_error = so->so_error;
	xso->so_pgid = so->so_pgid;
	xso->so_oobmark = so->so_oobmark;
	xso->so_uid = kauth_cred_getuid(so->so_cred);
	xso->so_last_pid = so->last_pid;
	xso->so_e_pid = so->e_pid;
	xso->so_gencnt = so->so_gencnt;
	xso->so_flags = so->so_flags;
	xso->so_flags1 = so->so_flags1;
	xso->so_usecount = so->so_usecount;
	xso->so_retaincnt = so->so_retaincnt;
	if (so->so_filt != NULL) {
		xso->xso_filter_flags |= XSOFF_SO_FILT;
	}
	if (so->so_flow_db != NULL) {
		xso->xso_filter_flags |= XSOFF_FLOW_DB;
	}
	if (so->so_cfil != NULL) {
		xso->xso_filter_flags |= XSOFF_CFIL;
	}
	if (so->so_fd_pcb != NULL) {
		xso->xso_filter_flags |= XSOFF_FLOW_DIV;
	}
}

__private_extern__ void
sbtoxsockbuf_n(struct sockbuf *sb, struct xsockbuf_n *xsb)
{
	xsb->xsb_len = sizeof(struct xsockbuf_n);

	if (sb == NULL) {
		return;
	}

	xsb->xsb_kind = (sb->sb_flags & SB_RECV) ? XSO_RCVBUF : XSO_SNDBUF;
	xsb->sb_cc = sb->sb_cc;
	xsb->sb_hiwat = sb->sb_hiwat;
	xsb->sb_mbcnt = sb->sb_mbcnt;
	xsb->sb_mbmax = sb->sb_mbmax;
	xsb->sb_lowat = sb->sb_lowat;
	xsb->sb_flags = (short)sb->sb_flags;
	xsb->sb_timeo = (short)((sb->sb_timeo.tv_sec * hz) +
	    sb->sb_timeo.tv_usec / tick);
	if (xsb->sb_timeo == 0 && sb->sb_timeo.tv_usec != 0) {
		xsb->sb_timeo = 1;
	}
}

__private_extern__ void
sbtoxsockstat_n(struct socket *so, struct xsockstat_n *xst)
{
	int i;

	xst->xst_len = sizeof(struct xsockstat_n);
	xst->xst_kind = XSO_STATS;

	if (so == NULL) {
		return;
	}

	for (i = 0; i < SO_TC_STATS_MAX; i++) {
		xst->xst_tc_stats[i].rxpackets = so->so_tc_stats[i].rxpackets;
		xst->xst_tc_stats[i].rxbytes = so->so_tc_stats[i].rxbytes;
		xst->xst_tc_stats[i].txpackets = so->so_tc_stats[i].txpackets;
		xst->xst_tc_stats[i].txbytes = so->so_tc_stats[i].txbytes;
	}
}

static void
inpcb_to_xinpcb_n(struct inpcb *inp, struct xinpcb_n *xinp)
{
	xinp->xi_len = sizeof(struct xinpcb_n);
	xinp->xi_kind = XSO_INPCB;
	xinp->xi_inpp = (uint64_t)VM_KERNEL_ADDRHASH(inp);
	xinp->inp_fport = inp->inp_fport;
	xinp->inp_lport = inp->inp_lport;
	xinp->inp_ppcb = (uint64_t)VM_KERNEL_ADDRHASH(inp->inp_ppcb);
	xinp->inp_gencnt = inp->inp_gencnt;
	xinp->inp_flags = inp->inp_flags;
	xinp->inp_flow = inp->inp_flow;
	xinp->inp_vflag = inp->inp_vflag;
	xinp->inp_ip_ttl = inp->inp_ip_ttl;
	xinp->inp_ip_p = inp->inp_ip_p;
	xinp->inp_dependfaddr.inp6_foreign = inp->inp_dependfaddr.inp6_foreign;
	xinp->inp_dependladdr.inp6_local = inp->inp_dependladdr.inp6_local;
	xinp->inp_depend4.inp4_ip_tos = inp->inp_depend4.inp4_ip_tos;
	xinp->inp_depend6.inp6_hlim = 0;
	xinp->inp_depend6.inp6_cksum = inp->inp_depend6.inp6_cksum;
	xinp->inp_depend6.inp6_ifindex = 0;
	xinp->inp_depend6.inp6_hops = inp->inp_depend6.inp6_hops;
	xinp->inp_flowhash = inp->inp_flowhash;
	xinp->inp_flags2 = inp->inp_flags2;
}

__private_extern__ void
tcpcb_to_xtcpcb_n(struct tcpcb *tp, struct xtcpcb_n *xt)
{
	xt->xt_len = sizeof(struct xtcpcb_n);
	xt->xt_kind = XSO_TCPCB;

	xt->t_segq = (uint32_t)VM_KERNEL_ADDRHASH(tp->t_segq.lh_first);
	xt->t_dupacks = tp->t_dupacks;
	xt->t_timer[TCPT_REXMT_EXT] = tp->t_timer[TCPT_REXMT];
	xt->t_timer[TCPT_PERSIST_EXT] = tp->t_timer[TCPT_PERSIST];
	xt->t_timer[TCPT_KEEP_EXT] = tp->t_timer[TCPT_KEEP];
	xt->t_timer[TCPT_2MSL_EXT] = tp->t_timer[TCPT_2MSL];
	xt->t_state = tp->t_state;
	xt->t_flags = tp->t_flags;
	xt->t_force = (tp->t_flagsext & TF_FORCE) ? 1 : 0;
	xt->snd_una = tp->snd_una;
	xt->snd_max = tp->snd_max;
	xt->snd_nxt = tp->snd_nxt;
	xt->snd_up = tp->snd_up;
	xt->snd_wl1 = tp->snd_wl1;
	xt->snd_wl2 = tp->snd_wl2;
	xt->iss = tp->iss;
	xt->irs = tp->irs;
	xt->rcv_nxt = tp->rcv_nxt;
	xt->rcv_adv = tp->rcv_adv;
	xt->rcv_wnd = tp->rcv_wnd;
	xt->rcv_up = tp->rcv_up;
	xt->snd_wnd = tp->snd_wnd;
	xt->snd_cwnd = tp->snd_cwnd;
	xt->snd_ssthresh = tp->snd_ssthresh;
	xt->t_maxopd = tp->t_maxopd;
	xt->t_rcvtime = tp->t_rcvtime;
	xt->t_starttime = tp->t_starttime;
	xt->t_rtttime = tp->t_rtttime;
	xt->t_rtseq = tp->t_rtseq;
	xt->t_rxtcur = tp->t_rxtcur;
	xt->t_maxseg = tp->t_maxseg;
	xt->t_srtt = tp->t_srtt;
	xt->t_rttvar = tp->t_rttvar;
	xt->t_rxtshift = tp->t_rxtshift;
	xt->t_rttmin = tp->t_rttmin;
	xt->t_rttupdated = tp->t_rttupdated;
	xt->max_sndwnd = tp->max_sndwnd;
	xt->t_softerror = tp->t_softerror;
	xt->t_oobflags = tp->t_oobflags;
	xt->t_iobc = tp->t_iobc;
	xt->snd_scale = tp->snd_scale;
	xt->rcv_scale = tp->rcv_scale;
	xt->request_r_scale = tp->request_r_scale;
	xt->requested_s_scale = tp->requested_s_scale;
	xt->ts_recent = tp->ts_recent;
	xt->ts_recent_age = tp->ts_recent_age;
	xt->last_ack_sent = tp->last_ack_sent;
	xt->cc_send = 0;
	xt->cc_recv = 0;
	xt->snd_recover = tp->snd_recover;
	xt->snd_cwnd_prev = tp->snd_cwnd_prev;
	xt->snd_ssthresh_prev = tp->snd_ssthresh_prev;
}

__private_extern__ int
get_pcblist_n(short proto, struct sysctl_req *req, struct inpcbinfo *pcbinfo)
{
	int error = 0;
	int i, n;
	size_t sz;
	struct inpcb *inp, **inp_list = NULL;
	inp_gen_t gencnt;
	struct xinpgen xig;
	void *buf = NULL;
	size_t item_size = ROUNDUP64(sizeof(struct xinpcb_n)) +
	    ROUNDUP64(sizeof(struct xsocket_n)) +
	    2 * ROUNDUP64(sizeof(struct xsockbuf_n)) +
	    ROUNDUP64(sizeof(struct xsockstat_n));
#if SKYWALK
	int nuserland;
	size_t userlandsnapshot_size = 0;
	void *__sized_by(userlandsnapshot_size) userlandsnapshot = NULL;
#endif /* SKYWALK */

	if (proto == IPPROTO_TCP) {
		item_size += ROUNDUP64(sizeof(struct xtcpcb_n));
	}

	if (req->oldptr == USER_ADDR_NULL) {
		n = pcbinfo->ipi_count;
#if SKYWALK
		n += ntstat_userland_count(proto);
#endif /* SKYWALK */
		req->oldidx = 2 * (sizeof(xig)) + (n + n / 8 + 1) * item_size;
		return 0;
	}

	if (req->newptr != USER_ADDR_NULL) {
		return EPERM;
	}

#if SKYWALK
	/*
	 * Get a snapshot of the state of the user level flows so we know
	 * the exact number of results to give back to the user.
	 * This could take a while and use other locks, so do this prior
	 * to taking any locks of our own.
	 */
	error = nstat_userland_get_snapshot(proto, &userlandsnapshot,
	    &userlandsnapshot_size, &nuserland);

	if (error) {
		return error;
	}
#endif /* SKYWALK */

	/*
	 * The process of preparing the PCB list is too time-consuming and
	 * resource-intensive to repeat twice on every request.
	 */
	lck_rw_lock_exclusive(&pcbinfo->ipi_lock);
	/*
	 * OK, now we're committed to doing something.
	 */
	gencnt = pcbinfo->ipi_gencnt;
	n = sz = pcbinfo->ipi_count;

	bzero(&xig, sizeof(xig));
	xig.xig_len = sizeof(xig);
	xig.xig_count = n;
#if SKYWALK
	xig.xig_count += nuserland;
#endif /* SKYWALK */
	xig.xig_gen = gencnt;
	xig.xig_sogen = so_gencnt;
	error = SYSCTL_OUT(req, &xig, sizeof(xig));
	if (error) {
		goto done;
	}
	/*
	 * We are done if there is no pcb
	 */
	if (xig.xig_count == 0) {
		goto done;
	}

	buf = kalloc_data(item_size, Z_WAITOK);
	if (buf == NULL) {
		error = ENOMEM;
		goto done;
	}

	inp_list = kalloc_type(struct inpcb *, sz, Z_WAITOK);
	if (inp_list == NULL) {
		error = ENOMEM;
		goto done;
	}

	/*
	 * Special case TCP to include the connections in time wait
	 */
	if (proto == IPPROTO_TCP) {
		n = get_tcp_inp_list(inp_list, sz, gencnt);
	} else {
		for (inp = pcbinfo->ipi_listhead->lh_first, i = 0; inp && i < sz;
		    inp = inp->inp_list.le_next) {
			if (inp->inp_gencnt <= gencnt &&
			    inp->inp_state != INPCB_STATE_DEAD) {
				inp_list[i++] = inp;
			}
		}
		n = i;
	}


	error = 0;
	for (i = 0; i < n; i++) {
		inp = inp_list[i];
		if (inp->inp_gencnt <= gencnt &&
		    inp->inp_state != INPCB_STATE_DEAD) {
			struct xinpcb_n *xi = (struct xinpcb_n *)buf;
			struct xsocket_n *xso = (struct xsocket_n *)
			    ADVANCE64(xi, sizeof(*xi));
			struct xsockbuf_n *xsbrcv = (struct xsockbuf_n *)
			    ADVANCE64(xso, sizeof(*xso));
			struct xsockbuf_n *xsbsnd = (struct xsockbuf_n *)
			    ADVANCE64(xsbrcv, sizeof(*xsbrcv));
			struct xsockstat_n *xsostats = (struct xsockstat_n *)
			    ADVANCE64(xsbsnd, sizeof(*xsbsnd));

			bzero(buf, item_size);

			inpcb_to_xinpcb_n(inp, xi);
			sotoxsocket_n(inp->inp_socket, xso);
			sbtoxsockbuf_n(inp->inp_socket ?
			    &inp->inp_socket->so_rcv : NULL, xsbrcv);
			sbtoxsockbuf_n(inp->inp_socket ?
			    &inp->inp_socket->so_snd : NULL, xsbsnd);
			sbtoxsockstat_n(inp->inp_socket, xsostats);
			if (proto == IPPROTO_TCP) {
				struct  xtcpcb_n *xt = (struct xtcpcb_n *)
				    ADVANCE64(xsostats, sizeof(*xsostats));

				/*
				 * inp->inp_ppcb, can only be NULL on
				 * an initialization race window.
				 * No need to lock.
				 */
				if (inp->inp_ppcb == NULL) {
					continue;
				}

				tcpcb_to_xtcpcb_n((struct tcpcb *)
				    inp->inp_ppcb, xt);
			}
			error = SYSCTL_OUT(req, buf, item_size);
			if (error) {
				break;
			}
		}
	}
#if SKYWALK
	if (!error && nuserland > 0) {
		error = nstat_userland_list_snapshot(proto, req, userlandsnapshot, nuserland);
	}
#endif /* SKYWALK */

	if (!error) {
		/*
		 * Give the user an updated idea of our state.
		 * If the generation differs from what we told
		 * her before, she knows that something happened
		 * while we were processing this request, and it
		 * might be necessary to retry.
		 */
		bzero(&xig, sizeof(xig));
		xig.xig_len = sizeof(xig);
		xig.xig_gen = pcbinfo->ipi_gencnt;
		xig.xig_sogen = so_gencnt;
		xig.xig_count = pcbinfo->ipi_count;
#if SKYWALK
		xig.xig_count +=  nuserland;
#endif /* SKYWALK */
		error = SYSCTL_OUT(req, &xig, sizeof(xig));
	}
done:
	lck_rw_done(&pcbinfo->ipi_lock);

#if SKYWALK
	nstat_userland_release_snapshot(userlandsnapshot, nuserland);
#endif /* SKYWALK */

	kfree_type(struct inpcb *, sz, inp_list);
	if (buf != NULL) {
		kfree_data(buf, item_size);
	}
	return error;
}

static void
inpcb_get_if_ports_used(ifnet_t ifp, int protocol, uint32_t flags,
    bitstr_t *__counted_by(bitstr_size(IP_PORTRANGE_SIZE)) bitfield, struct inpcbinfo *pcbinfo)
{
	struct inpcb *inp;
	struct socket *so;
	inp_gen_t gencnt;
	bool iswildcard, wildcardok, nowakeok;
	bool recvanyifonly, extbgidleok;
	bool activeonly;
	bool anytcpstateok;

	if (ifp == NULL) {
		return;
	}

	wildcardok = ((flags & IFNET_GET_LOCAL_PORTS_WILDCARDOK) != 0);
	nowakeok = ((flags & IFNET_GET_LOCAL_PORTS_NOWAKEUPOK) != 0);
	recvanyifonly = ((flags & IFNET_GET_LOCAL_PORTS_RECVANYIFONLY) != 0);
	extbgidleok = ((flags & IFNET_GET_LOCAL_PORTS_EXTBGIDLEONLY) != 0);
	activeonly = ((flags & IFNET_GET_LOCAL_PORTS_ACTIVEONLY) != 0);
	anytcpstateok = ((flags & IFNET_GET_LOCAL_PORTS_ANYTCPSTATEOK) != 0);

	lck_rw_lock_shared(&pcbinfo->ipi_lock);
	gencnt = pcbinfo->ipi_gencnt;

	for (inp = LIST_FIRST(pcbinfo->ipi_listhead); inp;
	    inp = LIST_NEXT(inp, inp_list)) {
		if (inp->inp_gencnt > gencnt ||
		    inp->inp_state == INPCB_STATE_DEAD ||
		    inp->inp_wantcnt == WNT_STOPUSING) {
			continue;
		}

		if ((so = inp->inp_socket) == NULL || inp->inp_lport == 0) {
			continue;
		}

		/*
		 * ANYTCPSTATEOK means incoming packets cannot be filtered
		 * reception so cast a wide net of possibilities
		 */
		if (!anytcpstateok &&
		    ((so->so_state & SS_DEFUNCT) ||
		    (so->so_state & SS_ISDISCONNECTED))) {
			continue;
		}

		/*
		 * If protocol is specified, filter out inpcbs that
		 * are not relevant to the protocol family of interest.
		 */
		if (protocol != PF_UNSPEC) {
			if (protocol == PF_INET) {
				/*
				 * If protocol of interest is IPv4, skip the inpcb
				 * if the family is not IPv4.
				 * OR
				 * If the family is IPv4, skip if the IPv4 flow is
				 * CLAT46 translated.
				 */
				if ((inp->inp_vflag & INP_IPV4) == 0 ||
				    (inp->inp_flags2 & INP2_CLAT46_FLOW) != 0) {
					continue;
				}
			} else if (protocol == PF_INET6) {
				/*
				 * If protocol of interest is IPv6, skip the inpcb
				 * if the family is not IPv6.
				 * AND
				 * The flow is not a CLAT46'd flow.
				 */
				if ((inp->inp_vflag & INP_IPV6) == 0 &&
				    (inp->inp_flags2 & INP2_CLAT46_FLOW) == 0) {
					continue;
				}
			} else {
				/* Protocol family not supported */
				continue;
			}
		}

		if (SOCK_PROTO(inp->inp_socket) != IPPROTO_UDP &&
		    SOCK_PROTO(inp->inp_socket) != IPPROTO_TCP) {
			continue;
		}

		iswildcard = (((inp->inp_vflag & INP_IPV4) &&
		    inp->inp_laddr.s_addr == INADDR_ANY) ||
		    ((inp->inp_vflag & INP_IPV6) &&
		    IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)));

		if (!wildcardok && iswildcard) {
			continue;
		}

		if (!(inp->inp_flags & INP_RECV_ANYIF) &&
		    recvanyifonly) {
			continue;
		}

		if (!(so->so_flags1 & SOF1_EXTEND_BK_IDLE_WANTED) &&
		    extbgidleok) {
			continue;
		}

		if (!iswildcard &&
		    !(inp->inp_last_outifp == NULL || ifp == inp->inp_last_outifp)) {
			continue;
		}

		if (!iswildcard && (ifp->if_eflags & IFEF_AWDL) != 0) {
			if (inp->inp_route.ro_rt == NULL ||
			    (inp->inp_route.ro_rt->rt_flags & (RTF_UP | RTF_CONDEMNED)) != RTF_UP) {
#if DEBUG || DEVELOPMENT
				char lbuf[MAX_IPv6_STR_LEN + 6] = {};
				char fbuf[MAX_IPv6_STR_LEN + 6] = {};
				char pname[MAXCOMLEN + 1];

				proc_name(so->last_pid, pname, sizeof(pname));

				if (protocol == PF_INET) {
					inet_ntop(PF_INET, &inp->inp_laddr.s_addr,
					    lbuf, sizeof(lbuf));
					inet_ntop(PF_INET, &inp->inp_faddr.s_addr,
					    fbuf, sizeof(fbuf));
				} else {
					inet_ntop(PF_INET6, &inp->in6p_laddr.s6_addr,
					    lbuf, sizeof(lbuf));
					inet_ntop(PF_INET6, &inp->in6p_faddr.s6_addr,
					    fbuf, sizeof(fbuf));
				}

				os_log(OS_LOG_DEFAULT,
				    "inpcb_get_if_ports_used: route is down %s %s:%u %s:%u ifp %s proc %s:%d",
				    SOCK_PROTO(inp->inp_socket) == IPPROTO_TCP ? "tcp" : "udp",
				    lbuf, ntohs(inp->inp_lport), fbuf, ntohs(inp->inp_fport),
				    ifp->if_xname, pname, so->last_pid);
#endif /* DEBUG || DEVELOPMENT */
				continue;
			}
		}

		if (SOCK_PROTO(inp->inp_socket) == IPPROTO_UDP &&
		    so->so_state & SS_CANTRCVMORE) {
			continue;
		}

		if (SOCK_PROTO(inp->inp_socket) == IPPROTO_TCP) {
			struct  tcpcb *tp = sototcpcb(inp->inp_socket);

			/*
			 * Workaround race where inp_ppcb is NULL during
			 * socket initialization
			 */
			if (tp == NULL) {
				continue;
			}

			switch (tp->t_state) {
			case TCPS_CLOSED:
				if (anytcpstateok && inp->inp_fport != 0) {
					/*
					 * A foreign port means we had a 4 tuple at
					 * least a connection attempt so packets
					 * may be received for the 4 tuple after the
					 * connection is gone
					 */
					break;
				}
				continue;
			/* NOT REACHED */
			case TCPS_LISTEN:
			case TCPS_SYN_SENT:
			case TCPS_SYN_RECEIVED:
			case TCPS_ESTABLISHED:
			case TCPS_FIN_WAIT_1:
				/*
				 * Note: FIN_WAIT_1 is an active state
				 * because we need our FIN to be
				 * acknowledged
				 */
				break;
			case TCPS_CLOSE_WAIT:
			case TCPS_CLOSING:
			case TCPS_LAST_ACK:
			case TCPS_FIN_WAIT_2:
				/*
				 * In the closing states, the connection
				 * is active when there is outgoing
				 * data having to be acknowledged
				 */
				if (!anytcpstateok &&
				    (activeonly && so->so_snd.sb_cc == 0)) {
					continue;
				}
				break;
			case TCPS_TIME_WAIT:
				if (anytcpstateok) {
					/*
					 * Packets may still be received for the 4 tuple
					 * after the connection is gone
					 */
					break;
				}
				continue;
				/* NOT REACHED */
			}
		}

#if DEBUG || DEVELOPMENT
		if ((so->so_options & SO_NOWAKEFROMSLEEP) && !nowakeok) {
			char lbuf[MAX_IPv6_STR_LEN + 6] = {};
			char fbuf[MAX_IPv6_STR_LEN + 6] = {};
			char pname[MAXCOMLEN + 1];

			proc_name(so->last_pid, pname, sizeof(pname));

			if (protocol == PF_INET) {
				inet_ntop(PF_INET, &inp->inp_laddr.s_addr,
				    lbuf, sizeof(lbuf));
				inet_ntop(PF_INET, &inp->inp_faddr.s_addr,
				    fbuf, sizeof(fbuf));
			} else {
				inet_ntop(PF_INET6, &inp->in6p_laddr.s6_addr,
				    lbuf, sizeof(lbuf));
				inet_ntop(PF_INET6, &inp->in6p_faddr.s6_addr,
				    fbuf, sizeof(fbuf));
			}

			os_log(OS_LOG_DEFAULT,
			    "inpcb_get_if_ports_used: no wake from sleep %s %s:%u %s:%u ifp %s proc %s:%d",
			    SOCK_PROTO(inp->inp_socket) == IPPROTO_TCP ? "tcp" : "udp",
			    lbuf, ntohs(inp->inp_lport), fbuf, ntohs(inp->inp_fport),
			    ifp->if_xname, pname, so->last_pid);
		}
#endif /* DEBUG || DEVELOPMENT */


		/*
		 * When the socket has "no wake from sleep" option, do not set the port in the bitmap
		 * except if explicetely requested by the driver.
		 * We always add the socket to the list of port in order to report spurious wakes
		 */
		if ((so->so_options & SO_NOWAKEFROMSLEEP) == 0 || nowakeok) {
			bitstr_set(bitfield, ntohs(inp->inp_lport));
		}

		(void) if_ports_used_add_inpcb(ifp->if_index, inp);
	}
	lck_rw_done(&pcbinfo->ipi_lock);
}

__private_extern__ void
inpcb_get_ports_used(ifnet_t ifp, int protocol, uint32_t flags,
    bitstr_t *__counted_by(bitstr_size(IP_PORTRANGE_SIZE)) bitfield, struct inpcbinfo *pcbinfo)
{
	if (ifp != NULL) {
		inpcb_get_if_ports_used(ifp, protocol, flags, bitfield, pcbinfo);
	} else {
		errno_t error;
		uint32_t count = 0;
		ifnet_t *__counted_by(count) ifp_list = NULL;
		uint32_t i;

		error = ifnet_list_get_all(IFNET_FAMILY_ANY, &ifp_list, &count);
		if (error != 0) {
			os_log_error(OS_LOG_DEFAULT,
			    "%s: ifnet_list_get_all() failed %d",
			    __func__, error);
			return;
		}
		for (i = 0; i < count; i++) {
			if (TAILQ_EMPTY(&ifp_list[i]->if_addrhead)) {
				continue;
			}
			inpcb_get_if_ports_used(ifp_list[i], protocol, flags,
			    bitfield, pcbinfo);
		}
		ifnet_list_free_counted_by(ifp_list, count);
	}
}

__private_extern__ uint32_t
inpcb_count_opportunistic(unsigned int ifindex, struct inpcbinfo *pcbinfo,
    u_int32_t flags)
{
	uint32_t opportunistic = 0;
	struct inpcb *inp;
	inp_gen_t gencnt;

	lck_rw_lock_shared(&pcbinfo->ipi_lock);
	gencnt = pcbinfo->ipi_gencnt;
	for (inp = LIST_FIRST(pcbinfo->ipi_listhead);
	    inp != NULL; inp = LIST_NEXT(inp, inp_list)) {
		if (inp->inp_gencnt <= gencnt &&
		    inp->inp_state != INPCB_STATE_DEAD &&
		    inp->inp_socket != NULL &&
		    so_get_opportunistic(inp->inp_socket) &&
		    inp->inp_last_outifp != NULL &&
		    ifindex == inp->inp_last_outifp->if_index) {
			opportunistic++;
			struct socket *so = inp->inp_socket;
			if ((flags & INPCB_OPPORTUNISTIC_SETCMD) &&
			    (so->so_state & SS_ISCONNECTED)) {
				socket_lock(so, 1);
				if (flags & INPCB_OPPORTUNISTIC_THROTTLEON) {
					so->so_flags |= SOF_SUSPENDED;
					soevent(so,
					    (SO_FILT_HINT_LOCKED |
					    SO_FILT_HINT_SUSPEND));
				} else {
					so->so_flags &= ~(SOF_SUSPENDED);
					soevent(so,
					    (SO_FILT_HINT_LOCKED |
					    SO_FILT_HINT_RESUME));
				}
				SOTHROTTLELOG("throttle[%d]: so 0x%llx "
				    "[%d,%d] %s\n", so->last_pid,
				    (uint64_t)VM_KERNEL_ADDRHASH(so),
				    SOCK_DOM(so), SOCK_TYPE(so),
				    (so->so_flags & SOF_SUSPENDED) ?
				    "SUSPENDED" : "RESUMED");
				socket_unlock(so, 1);
			}
		}
	}

	lck_rw_done(&pcbinfo->ipi_lock);

	return opportunistic;
}

__private_extern__ uint32_t
inpcb_find_anypcb_byaddr(struct ifaddr *ifa, struct inpcbinfo *pcbinfo)
{
	struct inpcb *inp;
	inp_gen_t gencnt = pcbinfo->ipi_gencnt;
	struct socket *so = NULL;
	int af;

	if ((ifa->ifa_addr->sa_family != AF_INET) &&
	    (ifa->ifa_addr->sa_family != AF_INET6)) {
		return 0;
	}

	lck_rw_lock_shared(&pcbinfo->ipi_lock);
	for (inp = LIST_FIRST(pcbinfo->ipi_listhead);
	    inp != NULL; inp = LIST_NEXT(inp, inp_list)) {
		if (inp->inp_gencnt <= gencnt &&
		    inp->inp_state != INPCB_STATE_DEAD &&
		    inp->inp_socket != NULL) {
			so = inp->inp_socket;
			af = SOCK_DOM(so);
			if (af != ifa->ifa_addr->sa_family) {
				continue;
			}
			if (inp->inp_last_outifp != ifa->ifa_ifp) {
				continue;
			}

			if (af == AF_INET) {
				if (inp->inp_laddr.s_addr ==
				    (satosin(ifa->ifa_addr))->sin_addr.s_addr) {
					lck_rw_done(&pcbinfo->ipi_lock);
					return 1;
				}
			}
			if (af == AF_INET6) {
				if (in6_are_addr_equal_scoped(IFA_IN6(ifa), &inp->in6p_laddr, SIN6(ifa->ifa_addr)->sin6_scope_id, inp->inp_lifscope)) {
					lck_rw_done(&pcbinfo->ipi_lock);
					return 1;
				}
			}
		}
	}
	lck_rw_done(&pcbinfo->ipi_lock);
	return 0;
}

static int
shutdown_sockets_on_interface_proc_callout(proc_t p, void *arg)
{
	struct fileproc *fp;
	struct ifnet *ifp = (struct ifnet *)arg;

	if (ifp == NULL) {
		return PROC_RETURNED;
	}

	proc_fdlock(p);

	fdt_foreach(fp, p) {
		struct fileglob *fg = fp->fp_glob;
		struct socket *so;
		struct inpcb *inp;
		struct ifnet *inp_ifp;
		int error;

		if (FILEGLOB_DTYPE(fg) != DTYPE_SOCKET) {
			continue;
		}

		so = (struct socket *)fp_get_data(fp);
		if (SOCK_DOM(so) != PF_INET && SOCK_DOM(so) != PF_INET6) {
			continue;
		}

		inp = (struct inpcb *)so->so_pcb;

		if (in_pcb_checkstate(inp, WNT_ACQUIRE, 0) == WNT_STOPUSING) {
			continue;
		}

		socket_lock(so, 1);

		if (in_pcb_checkstate(inp, WNT_RELEASE, 1) == WNT_STOPUSING) {
			socket_unlock(so, 1);
			continue;
		}

		if (inp->inp_boundifp != NULL) {
			inp_ifp = inp->inp_boundifp;
		} else if (inp->inp_last_outifp != NULL) {
			inp_ifp = inp->inp_last_outifp;
		} else {
			socket_unlock(so, 1);
			continue;
		}

		if (inp_ifp != ifp && inp_ifp->if_delegated.ifp != ifp) {
			socket_unlock(so, 1);
			continue;
		}
		error = sosetdefunct(p, so, 0, TRUE);
		if (error != 0) {
			log(LOG_ERR, "%s: sosetdefunct() error %d",
			    __func__, error);
		} else {
			error = sodefunct(p, so, 0);
			if (error != 0) {
				log(LOG_ERR, "%s: sodefunct() error %d",
				    __func__, error);
			}
		}

		socket_unlock(so, 1);
	}
	proc_fdunlock(p);

	return PROC_RETURNED;
}

void
shutdown_sockets_on_interface(struct ifnet *ifp)
{
	proc_iterate(PROC_ALLPROCLIST,
	    shutdown_sockets_on_interface_proc_callout,
	    ifp, NULL, NULL);
}

__private_extern__ int
inp_limit_companion_link(struct inpcbinfo *pcbinfo, u_int32_t limit)
{
	struct inpcb *inp;
	struct socket *so = NULL;

	lck_rw_lock_shared(&pcbinfo->ipi_lock);
	inp_gen_t gencnt = pcbinfo->ipi_gencnt;
	for (inp = LIST_FIRST(pcbinfo->ipi_listhead);
	    inp != NULL; inp = LIST_NEXT(inp, inp_list)) {
		if (inp->inp_gencnt <= gencnt &&
		    inp->inp_state != INPCB_STATE_DEAD &&
		    inp->inp_socket != NULL) {
			so = inp->inp_socket;

			if ((so->so_state & SS_DEFUNCT) || so->so_state & SS_ISDISCONNECTED ||
			    SOCK_PROTO(so) != IPPROTO_TCP || inp->inp_last_outifp == NULL ||
			    !IFNET_IS_COMPANION_LINK(inp->inp_last_outifp)) {
				continue;
			}
			so->so_snd.sb_flags &= ~SB_LIMITED;
			u_int32_t new_size = MAX(MIN(limit, so->so_snd.sb_lowat), so->so_snd.sb_cc);
			sbreserve(&so->so_snd, new_size);
			so->so_snd.sb_flags |= SB_LIMITED;
		}
	}
	lck_rw_done(&pcbinfo->ipi_lock);
	return 0;
}

__private_extern__ int
inp_recover_companion_link(struct inpcbinfo *pcbinfo)
{
	struct inpcb *inp;
	inp_gen_t gencnt = pcbinfo->ipi_gencnt;
	struct socket *so = NULL;

	lck_rw_lock_shared(&pcbinfo->ipi_lock);
	for (inp = LIST_FIRST(pcbinfo->ipi_listhead);
	    inp != NULL; inp = LIST_NEXT(inp, inp_list)) {
		if (inp->inp_gencnt <= gencnt &&
		    inp->inp_state != INPCB_STATE_DEAD &&
		    inp->inp_socket != NULL) {
			so = inp->inp_socket;

			if (SOCK_PROTO(so) != IPPROTO_TCP || inp->inp_last_outifp == NULL ||
			    !(so->so_snd.sb_flags & SB_LIMITED)) {
				continue;
			}

			so->so_snd.sb_flags &= ~SB_LIMITED;
		}
	}
	lck_rw_done(&pcbinfo->ipi_lock);
	return 0;
}
