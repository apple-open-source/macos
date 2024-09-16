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
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
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
 *	@(#)tcp_input.c	8.12 (Berkeley) 5/24/95
 * $FreeBSD: src/sys/netinet/tcp_input.c,v 1.107.2.16 2001/08/22 00:59:12 silby Exp $
 */
/*
 * NOTICE: This file was modified by SPARTA, Inc. in 2005 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 */

#include "tcp_includes.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>           /* for proc0 declaration */
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <sys/mcache.h>
#include <sys/kauth.h>
#include <kern/cpu_number.h>    /* before tcp_seq.h, for tcp_random18() */

#include <machine/endian.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/ntstat.h>
#include <net/content_filter.h>
#include <net/dlil.h>
#include <net/multi_layer_pkt_log.h>
#include <net/droptap.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>    /* for ICMP_BANDLIM		*/
#include <netinet/in_var.h>
#include <netinet/icmp_var.h>   /* for ICMP_BANDLIM	*/
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <mach/sdt.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_cache.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_cc.h>
#include <dev/random/randomdev.h>
#include <kern/zalloc.h>
#include <netinet6/tcp6_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_log.h>

#if IPSEC
#include <netinet6/ipsec.h>
#include <netinet6/ipsec6.h>
#include <netkey/key.h>
#endif /*IPSEC*/

#include <sys/kdebug.h>
#if MPTCP
#include <netinet/mptcp_var.h>
#include <netinet/mptcp.h>
#include <netinet/mptcp_opt.h>
#endif /* MPTCP */

#include <corecrypto/ccaes.h>
#include <net/sockaddr_utils.h>

#define DBG_LAYER_BEG           NETDBG_CODE(DBG_NETTCP, 0)
#define DBG_LAYER_END           NETDBG_CODE(DBG_NETTCP, 2)
#define DBG_FNC_TCP_INPUT       NETDBG_CODE(DBG_NETTCP, (3 << 8))
#define DBG_FNC_TCP_NEWCONN     NETDBG_CODE(DBG_NETTCP, (7 << 8))

#define TCP_RTT_HISTORY_EXPIRE_TIME     (60 * TCP_RETRANSHZ)
#define TCP_RECV_THROTTLE_WIN   (5 * TCP_RETRANSHZ)
#define TCP_STRETCHACK_ENABLE_PKTCNT    2000

struct  tcpstat tcpstat;

SYSCTL_SKMEM_TCP_INT(OID_AUTO, flow_control_response,
    CTLFLAG_RW | CTLFLAG_LOCKED, int, tcp_flow_control_response, 1,
    "Improved response to Flow-control events");

static int log_in_vain = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, log_in_vain,
    CTLFLAG_RW | CTLFLAG_LOCKED, &log_in_vain, 0,
    "Log all incoming TCP connections");

SYSCTL_SKMEM_TCP_INT(OID_AUTO, ack_strategy,
    CTLFLAG_RW | CTLFLAG_LOCKED, int, tcp_ack_strategy, TCP_ACK_STRATEGY_MODERN,
    "Revised TCP ACK-strategy, avoiding stretch-ACK implementation");

static int blackhole = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, blackhole,
    CTLFLAG_RW | CTLFLAG_LOCKED, &blackhole, 0,
    "Do not send RST when dropping refused connections");

/* TODO - remove once uTCP stopped using it */
SYSCTL_SKMEM_TCP_INT(OID_AUTO, aggressive_rcvwnd_inc,
    CTLFLAG_RW | CTLFLAG_LOCKED, int, tcp_aggressive_rcvwnd_inc, 1,
    "Be more aggressive about increasing the receive-window.");

SYSCTL_SKMEM_TCP_INT(OID_AUTO, delayed_ack,
    CTLFLAG_RW | CTLFLAG_LOCKED, int, tcp_delack_enabled, 3,
    "Delay ACK to try and piggyback it onto a data packet");

SYSCTL_SKMEM_TCP_INT(OID_AUTO, recvbg, CTLFLAG_RW | CTLFLAG_LOCKED,
    int, tcp_recv_bg, 0, "Receive background");

SYSCTL_SKMEM_TCP_INT(OID_AUTO, drop_synfin,
    CTLFLAG_RW | CTLFLAG_LOCKED, static int, drop_synfin, 1,
    "Drop TCP packets with SYN+FIN set");

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, reass, CTLFLAG_RW | CTLFLAG_LOCKED, 0,
    "TCP Segment Reassembly Queue");

static int tcp_reass_overflows = 0;
SYSCTL_INT(_net_inet_tcp_reass, OID_AUTO, overflows,
    CTLFLAG_RD | CTLFLAG_LOCKED, &tcp_reass_overflows, 0,
    "Global number of TCP segment reassembly queue overflows");

int tcp_reass_total_qlen = 0;
SYSCTL_INT(_net_inet_tcp_reass, OID_AUTO, qlen,
    CTLFLAG_RD | CTLFLAG_LOCKED, &tcp_reass_total_qlen, 0,
    "Total number of TCP segments in reassembly queues");


SYSCTL_SKMEM_TCP_INT(OID_AUTO, slowlink_wsize, CTLFLAG_RW | CTLFLAG_LOCKED,
    __private_extern__ int, slowlink_wsize, 8192,
    "Maximum advertised window size for slowlink");

SYSCTL_SKMEM_TCP_INT(OID_AUTO, maxseg_unacked,
    CTLFLAG_RW | CTLFLAG_LOCKED, int, maxseg_unacked, 8,
    "Maximum number of outstanding segments left unacked");

SYSCTL_SKMEM_TCP_INT(OID_AUTO, rfc3465, CTLFLAG_RW | CTLFLAG_LOCKED,
    int, tcp_do_rfc3465, 1, "");

SYSCTL_SKMEM_TCP_INT(OID_AUTO, rfc3465_lim2,
    CTLFLAG_RW | CTLFLAG_LOCKED, int, tcp_do_rfc3465_lim2, 1,
    "Appropriate bytes counting w/ L=2*SMSS");

int rtt_samples_per_slot = 20;

int tcp_acc_iaj_high_thresh = ACC_IAJ_HIGH_THRESH;
u_int32_t tcp_autorcvbuf_inc_shift = 3;
SYSCTL_SKMEM_TCP_INT(OID_AUTO, recv_allowed_iaj,
    CTLFLAG_RW | CTLFLAG_LOCKED, int, tcp_allowed_iaj, ALLOWED_IAJ,
    "Allowed inter-packet arrival jiter");

SYSCTL_SKMEM_TCP_INT(OID_AUTO, doautorcvbuf,
    CTLFLAG_RW | CTLFLAG_LOCKED, u_int32_t, tcp_do_autorcvbuf, 1,
    "Enable automatic socket buffer tuning");

SYSCTL_SKMEM_TCP_INT(OID_AUTO, autotunereorder,
    CTLFLAG_RW | CTLFLAG_LOCKED, u_int32_t, tcp_autotune_reorder, 1,
    "Enable automatic socket buffer tuning even when reordering is present");

SYSCTL_SKMEM_TCP_INT(OID_AUTO, autorcvbufmax,
    CTLFLAG_RW | CTLFLAG_LOCKED | CTLFLAG_KERN, u_int32_t, tcp_autorcvbuf_max, 2 * 1024 * 1024,
    "Maximum receive socket buffer size");

int tcp_disable_access_to_stats = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, disable_access_to_stats,
    CTLFLAG_RW | CTLFLAG_LOCKED, &tcp_disable_access_to_stats, 0,
    "Disable access to tcpstat");

SYSCTL_SKMEM_TCP_INT(OID_AUTO, challengeack_limit,
    CTLFLAG_RW | CTLFLAG_LOCKED, uint32_t, tcp_challengeack_limit, 10,
    "Maximum number of challenge ACKs per connection per second");

/* TO BE REMOVED */
SYSCTL_SKMEM_TCP_INT(OID_AUTO, do_rfc5961,
    CTLFLAG_RW | CTLFLAG_LOCKED, static int, tcp_do_rfc5961, 1,
    "Enable/Disable full RFC 5961 compliance");

SYSCTL_SKMEM_TCP_INT(OID_AUTO, do_better_lr,
    CTLFLAG_RW | CTLFLAG_LOCKED, int, tcp_do_better_lr, 1,
    "Improved TCP Loss Recovery");

SYSCTL_SKMEM_TCP_INT(OID_AUTO, use_min_curr_rtt,
    CTLFLAG_RW | CTLFLAG_LOCKED, int, tcp_use_min_curr_rtt, 1,
    "Use a min of k=4 RTT samples for congestion controllers");

SYSCTL_SKMEM_TCP_INT(OID_AUTO, awdl_rtobase,
    CTLFLAG_RW | CTLFLAG_LOCKED, int, tcp_awdl_rtobase, 100,
    "Initial RTO for AWDL interface");

extern int tcp_acc_iaj_high;
extern int tcp_acc_iaj_react_limit;
extern int tcp_fin_timeout;

uint8_t tcprexmtthresh = 3;

uint32_t tcp_now;
struct timeval tcp_uptime;      /* uptime when tcp_now was last updated */

/* Used to sychronize updates to tcp_now */
static LCK_GRP_DECLARE(tcp_uptime_mtx_grp, "tcpuptime");
LCK_SPIN_DECLARE(tcp_uptime_lock, &tcp_uptime_mtx_grp);

struct inpcbhead tcb;
#define tcb6    tcb  /* for KAME src sync over BSD*'s */
struct inpcbinfo tcbinfo;

static void tcp_dooptions(struct tcpcb *, u_char *, int, struct tcphdr *,
    struct tcpopt *);
static void tcp_finalize_options(struct tcpcb *, struct tcpopt *, unsigned int);
static void tcp_pulloutofband(struct socket *,
    struct tcphdr *, struct mbuf *, int);
static void tcp_xmit_timer(struct tcpcb *, int, u_int32_t, tcp_seq);
static inline unsigned int tcp_maxmtu(struct rtentry *);
static inline int tcp_stretch_ack_enable(struct tcpcb *tp, int thflags);
static inline void tcp_adaptive_rwtimo_check(struct tcpcb *, int);

#if TRAFFIC_MGT
static inline void compute_iaj(struct tcpcb *tp);
static inline void compute_iaj_meat(struct tcpcb *tp, uint32_t cur_iaj);
#endif /* TRAFFIC_MGT */

static inline unsigned int tcp_maxmtu6(struct rtentry *);
unsigned int get_maxmtu(struct rtentry *);

static void tcp_sbrcv_grow(struct tcpcb *tp, struct sockbuf *sb,
    struct tcpopt *to, uint32_t tlen);
void tcp_sbrcv_trim(struct tcpcb *tp, struct sockbuf *sb);
static void tcp_sbsnd_trim(struct sockbuf *sbsnd);
static inline void tcp_sbrcv_tstmp_check(struct tcpcb *tp);
static inline void tcp_sbrcv_reserve(struct tcpcb *tp, struct sockbuf *sb,
    u_int32_t newsize, u_int32_t idealsize, u_int32_t rcvbuf_max);
static void tcp_bad_rexmt_restore_state(struct tcpcb *tp, struct tcphdr *th);
static void tcp_compute_rtt(struct tcpcb *tp, struct tcpopt *to,
    struct tcphdr *th);
static void tcp_compute_rcv_rtt(struct tcpcb *tp, struct tcpopt *to,
    struct tcphdr *th);
static void tcp_early_rexmt_check(struct tcpcb *tp, struct tcphdr *th);
static void tcp_bad_rexmt_check(struct tcpcb *tp, struct tcphdr *th,
    struct tcpopt *to);
/*
 * Constants used for resizing receive socket buffer
 * when timestamps are not supported
 */
#define TCPTV_RCVNOTS_QUANTUM 100
#define TCP_RCVNOTS_BYTELEVEL 204800

/*
 * Constants used for limiting early retransmits
 * to 10 per minute.
 */
#define TCP_EARLY_REXMT_WIN (60 * TCP_RETRANSHZ) /* 60 seconds */
#define TCP_EARLY_REXMT_LIMIT 10

#define log_in_vain_log( a ) { log a; }

int tcp_rcvunackwin = TCPTV_UNACKWIN;
int tcp_maxrcvidle = TCPTV_MAXRCVIDLE;
SYSCTL_SKMEM_TCP_INT(OID_AUTO, rcvsspktcnt, CTLFLAG_RW | CTLFLAG_LOCKED,
    int, tcp_rcvsspktcnt, TCP_RCV_SS_PKTCOUNT, "packets to be seen before receiver stretches acks");

#define DELAY_ACK(tp, th) \
	(CC_ALGO(tp)->delay_ack != NULL && CC_ALGO(tp)->delay_ack(tp, th))

static int tcp_dropdropablreq(struct socket *head);
static void tcp_newreno_partial_ack(struct tcpcb *tp, struct tcphdr *th);
static void update_base_rtt(struct tcpcb *tp, uint32_t rtt);
void tcp_set_background_cc(struct socket *so);
void tcp_set_foreground_cc(struct socket *so);
static void tcp_set_new_cc(struct socket *so, uint8_t cc_index);
static void tcp_bwmeas_check(struct tcpcb *tp);

#if TRAFFIC_MGT
void
reset_acc_iaj(struct tcpcb *tp)
{
	tp->acc_iaj = 0;
	CLEAR_IAJ_STATE(tp);
}

static inline void
update_iaj_state(struct tcpcb *tp, int size, int rst_size)
{
	if (rst_size > 0) {
		tp->iaj_size = 0;
	}
	if (tp->iaj_size == 0 || size >= tp->iaj_size) {
		tp->iaj_size = size;
		tp->iaj_rcv_ts = tcp_now;
		tp->iaj_small_pkt = 0;
	}
}

/* For every 64-bit unsigned integer(v), this function will find the
 * largest 32-bit integer n such that (n*n <= v). This takes at most 32 iterations
 * irrespective of the value of v and does not involve multiplications.
 */
static inline uint32_t
isqrt(uint64_t val)
{
	uint32_t sqrt_cache[11] = {0, 1, 4, 9, 16, 25, 36, 49, 64, 81, 100};
	uint64_t temp, g = 0, b = 1 << 31, bshft = 31;
	if (val <= 100) {
		for (g = 0; g <= 10; ++g) {
			if (sqrt_cache[g] > val) {
				g--;
				break;
			} else if (sqrt_cache[g] == val) {
				break;
			}
		}
	} else {
		do {
			temp = (((g << 1) + b) << (bshft--));
			if (val >= temp) {
				g += b;
				val -= temp;
			}
			b >>= 1;
		} while (b > 0 && val > 0);
	}
	return (uint32_t)g;
}

static inline void
compute_iaj_meat(struct tcpcb *tp, uint32_t cur_iaj)
{
	/* When accumulated IAJ reaches MAX_ACC_IAJ in milliseconds,
	 * throttle the receive window to a minimum of MIN_IAJ_WIN packets
	 */
#define MAX_ACC_IAJ (tcp_acc_iaj_high_thresh + tcp_acc_iaj_react_limit)
#define IAJ_DIV_SHIFT 4
#define IAJ_ROUNDUP_CONST (1 << (IAJ_DIV_SHIFT - 1))

	uint32_t allowed_iaj, acc_iaj = 0;

	/* Using 64-bit storage for the inter-arrival jitter deviation,
	 * to avoid accidentally rolling over if the inter-arrival time exceeds 62 seconds.
	 */
	int64_t mean, temp, cur_iaj_dev;

	cur_iaj_dev = (cur_iaj - tp->avg_iaj);

	/* Allow a jitter of "allowed_iaj" milliseconds. Some connections
	 * may have a constant jitter more than that. We detect this by
	 * using standard deviation.
	 */
	allowed_iaj = tp->avg_iaj + tp->std_dev_iaj;
	if (allowed_iaj < tcp_allowed_iaj) {
		allowed_iaj = tcp_allowed_iaj;
	}

	/* Initially when the connection starts, the senders congestion
	 * window is small. During this period we avoid throttling a
	 * connection because we do not have a good starting point for
	 * allowed_iaj. IAJ_IGNORE_PKTCNT is used to quietly gloss over
	 * the first few packets.
	 */
	if (tp->iaj_pktcnt > IAJ_IGNORE_PKTCNT) {
		if (cur_iaj <= allowed_iaj) {
			if (tp->acc_iaj >= 2) {
				acc_iaj = tp->acc_iaj - 2;
			} else {
				acc_iaj = 0;
			}
		} else {
			acc_iaj = tp->acc_iaj + (cur_iaj - allowed_iaj);
		}

		if (acc_iaj > MAX_ACC_IAJ) {
			acc_iaj = MAX_ACC_IAJ;
		}
		tp->acc_iaj = acc_iaj;
	}

	/* Compute weighted average where the history has a weight of
	 * 15 out of 16 and the current value has a weight of 1 out of 16.
	 * This will make the short-term measurements have more weight.
	 *
	 * The addition of 8 will help to round-up the value
	 * instead of round-down
	 */
	tp->avg_iaj = (((tp->avg_iaj << IAJ_DIV_SHIFT) - tp->avg_iaj)
	    + cur_iaj + IAJ_ROUNDUP_CONST) >> IAJ_DIV_SHIFT;

	/* Compute Root-mean-square of deviation where mean is a weighted
	 * average as described above.
	 */
	temp = tp->std_dev_iaj * tp->std_dev_iaj;
	mean = (((temp << IAJ_DIV_SHIFT) - temp)
	    + (cur_iaj_dev * cur_iaj_dev)
	    + IAJ_ROUNDUP_CONST) >> IAJ_DIV_SHIFT;

	tp->std_dev_iaj = isqrt(mean);

	DTRACE_TCP3(iaj, struct tcpcb *, tp, uint32_t, cur_iaj,
	    uint32_t, allowed_iaj);

	return;
}

static inline void
compute_iaj(struct tcpcb *tp)
{
	compute_iaj_meat(tp, (tcp_now - tp->iaj_rcv_ts));
}
#endif /* TRAFFIC_MGT */

/*
 * Perform rate limit check per connection per second
 * tp->t_challengeack_last is the last_time diff was greater than 1sec
 * tp->t_challengeack_count is the number of ACKs sent (within 1sec)
 * Return TRUE if we shouldn't send the ACK due to rate limitation
 * Return FALSE if it is still ok to send challenge ACK
 */
static boolean_t
tcp_is_ack_ratelimited(struct tcpcb *tp)
{
	boolean_t ret = TRUE;
	uint32_t now = tcp_now;
	int32_t diff = 0;

	diff = timer_diff(now, 0, tp->t_challengeack_last, 0);
	/* If it is first time or diff > 1000ms,
	 * update the challengeack_last and reset the
	 * current count of ACKs
	 */
	if (tp->t_challengeack_last == 0 || diff >= 1000) {
		tp->t_challengeack_last = now;
		tp->t_challengeack_count = 0;
		ret = FALSE;
	} else if (tp->t_challengeack_count < tcp_challengeack_limit) {
		ret = FALSE;
	}

	/* Careful about wrap-around */
	if (ret == FALSE && (tp->t_challengeack_count + 1 > 0)) {
		tp->t_challengeack_count++;
	}

	return ret;
}

/* Check if enough amount of data has been acknowledged since
 * bw measurement was started
 */
static void
tcp_bwmeas_check(struct tcpcb *tp)
{
	int32_t bw_meas_bytes;
	uint32_t bw, bytes, elapsed_time;

	if (SEQ_LEQ(tp->snd_una, tp->t_bwmeas->bw_start)) {
		return;
	}

	bw_meas_bytes = tp->snd_una - tp->t_bwmeas->bw_start;
	if ((tp->t_flagsext & TF_BWMEAS_INPROGRESS) &&
	    bw_meas_bytes >= (int32_t)(tp->t_bwmeas->bw_size)) {
		bytes = bw_meas_bytes;
		elapsed_time = tcp_now - tp->t_bwmeas->bw_ts;
		if (elapsed_time > 0) {
			bw = bytes / elapsed_time;
			if (bw > 0) {
				if (tp->t_bwmeas->bw_sndbw > 0) {
					tp->t_bwmeas->bw_sndbw =
					    (((tp->t_bwmeas->bw_sndbw << 3)
					    - tp->t_bwmeas->bw_sndbw)
					    + bw) >> 3;
				} else {
					tp->t_bwmeas->bw_sndbw = bw;
				}

				/* Store the maximum value */
				if (tp->t_bwmeas->bw_sndbw_max == 0) {
					tp->t_bwmeas->bw_sndbw_max =
					    tp->t_bwmeas->bw_sndbw;
				} else {
					tp->t_bwmeas->bw_sndbw_max =
					    max(tp->t_bwmeas->bw_sndbw,
					    tp->t_bwmeas->bw_sndbw_max);
				}
			}
		}
		tp->t_flagsext &= ~(TF_BWMEAS_INPROGRESS);
	}
}

static int
tcp_reass(struct tcpcb *tp, struct tcphdr *th, int *tlenp, struct mbuf *m,
    struct ifnet *ifp, int *dowakeup)
{
	struct tseg_qent *q;
	struct tseg_qent *p = NULL;
	struct tseg_qent *nq;
	struct tseg_qent *te = NULL;
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so = inp->inp_socket;
	int flags = 0;
	uint32_t qlimit;
	stats_functional_type ifnet_count_type = IFNET_COUNT_TYPE(ifp);
	boolean_t dsack_set = FALSE;

	/*
	 * If the reassembly queue already has entries or if we are going
	 * to add a new one, then the connection has reached a loss state.
	 * Reset the stretch-ack algorithm at this point.
	 */
	tcp_reset_stretch_ack(tp);
	tp->t_forced_acks = TCP_FORCED_ACKS_COUNT;

#if TRAFFIC_MGT
	if (tp->acc_iaj > 0) {
		reset_acc_iaj(tp);
	}
#endif /* TRAFFIC_MGT */

	if (th->th_seq != tp->rcv_nxt) {
		struct mbuf *tmp = m;
		while (tmp != NULL) {
			if (mbuf_class_under_pressure(tmp)) {
				m_freem(m);
				tcp_reass_overflows++;
				tcpstat.tcps_rcvmemdrop++;
				*tlenp = 0;
				return 0;
			}

			tmp = tmp->m_next;
		}
	}

	/*
	 * Limit the number of segments in the reassembly queue to prevent
	 * holding on to too many segments (and thus running out of mbufs).
	 * Make sure to let the missing segment through which caused this
	 * queue.  Always keep one global queue entry spare to be able to
	 * process the missing segment.
	 */
	qlimit = min(max(100, so->so_rcv.sb_hiwat >> 10),
	    (tcp_autorcvbuf_max >> 10));
	if (th->th_seq != tp->rcv_nxt &&
	    (tp->t_reassqlen + 1) >= qlimit) {
		tcp_reass_overflows++;
		tcpstat.tcps_rcvmemdrop++;
		m_freem(m);
		*tlenp = 0;
		return 0;
	}

	/* Allocate a new queue entry. If we can't, just drop the pkt. XXX */
	te = zalloc_flags(tcp_reass_zone, Z_WAITOK | Z_NOFAIL);
	tp->t_reassqlen++;
	OSIncrementAtomic(&tcp_reass_total_qlen);

	/*
	 * Find a segment which begins after this one does.
	 */
	LIST_FOREACH(q, &tp->t_segq, tqe_q) {
		if (SEQ_GT(q->tqe_th->th_seq, th->th_seq)) {
			break;
		}
		p = q;
	}

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us.
	 */
	if (p != NULL) {
		int i;
		/* conversion to int (in i) handles seq wraparound */
		i = p->tqe_th->th_seq + p->tqe_len - th->th_seq;
		if (i > 0) {
			if (i > 1) {
				/*
				 * Note duplicate data sequnce numbers
				 * to report in DSACK option
				 */
				tp->t_dsack_lseq = th->th_seq;
				tp->t_dsack_rseq = th->th_seq +
				    min(i, *tlenp);

				/*
				 * Report only the first part of partial/
				 * non-contiguous duplicate sequence space
				 */
				dsack_set = TRUE;
			}
			if (i >= *tlenp) {
				tcpstat.tcps_rcvduppack++;
				tcpstat.tcps_rcvdupbyte += *tlenp;
				if (nstat_collect) {
					nstat_route_rx(inp->inp_route.ro_rt,
					    1, *tlenp,
					    NSTAT_RX_FLAG_DUPLICATE);
					INP_ADD_STAT(inp, ifnet_count_type,
					    rxpackets, 1);
					INP_ADD_STAT(inp, ifnet_count_type,
					    rxbytes, *tlenp);
					tp->t_stat.rxduplicatebytes += *tlenp;
					inp_set_activity_bitmap(inp);
				}
				m_freem(m);
				zfree(tcp_reass_zone, te);
				te = NULL;
				tp->t_reassqlen--;
				OSDecrementAtomic(&tcp_reass_total_qlen);
				/*
				 * Try to present any queued data
				 * at the left window edge to the user.
				 * This is needed after the 3-WHS
				 * completes.
				 */
				goto present;
			}
			m_adj(m, i);
			*tlenp -= i;
			th->th_seq += i;
		}
	}

	if (th->th_seq != tp->rcv_nxt) {
		tp->t_rcvoopack++;
		tcpstat.tcps_rcvoopack++;
		tcpstat.tcps_rcvoobyte += *tlenp;
		if (nstat_collect) {
			tp->t_stat.rxoutoforderbytes += *tlenp;
		}
	}

	if (nstat_collect) {
		nstat_route_rx(inp->inp_route.ro_rt, 1, *tlenp,
		    NSTAT_RX_FLAG_OUT_OF_ORDER);
		INP_ADD_STAT(inp, ifnet_count_type, rxpackets, 1);
		INP_ADD_STAT(inp, ifnet_count_type, rxbytes, *tlenp);
		inp_set_activity_bitmap(inp);
	}

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 */
	while (q) {
		int i = (th->th_seq + *tlenp) - q->tqe_th->th_seq;
		if (i <= 0) {
			break;
		}

		/*
		 * Report only the first part of partial/non-contiguous
		 * duplicate segment in dsack option. The variable
		 * dsack_set will be true if a previous entry has some of
		 * the duplicate sequence space.
		 */
		if (i > 1 && !dsack_set) {
			if (tp->t_dsack_lseq == 0) {
				tp->t_dsack_lseq = q->tqe_th->th_seq;
				tp->t_dsack_rseq =
				    tp->t_dsack_lseq + min(i, q->tqe_len);
			} else {
				/*
				 * this segment overlaps data in multple
				 * entries in the reassembly queue, move
				 * the right sequence number further.
				 */
				tp->t_dsack_rseq =
				    tp->t_dsack_rseq + min(i, q->tqe_len);
			}
		}
		if (i < q->tqe_len) {
			q->tqe_th->th_seq += i;
			q->tqe_len -= i;
			m_adj(q->tqe_m, i);
			break;
		}

		nq = LIST_NEXT(q, tqe_q);
		LIST_REMOVE(q, tqe_q);
		tp->t_reassq_mbcnt -= _MSIZE + (q->tqe_m->m_flags & M_EXT) ?
		    q->tqe_m->m_ext.ext_size : 0;
		m_freem(q->tqe_m);
		zfree(tcp_reass_zone, q);
		tp->t_reassqlen--;
		OSDecrementAtomic(&tcp_reass_total_qlen);
		q = nq;
	}

	/* Insert the new segment queue entry into place. */
	te->tqe_m = m;
	te->tqe_th = th;
	te->tqe_len = *tlenp;

	tp->t_reassq_mbcnt += _MSIZE + (m->m_flags & M_EXT) ? m->m_ext.ext_size : 0;

	if (p == NULL) {
		LIST_INSERT_HEAD(&tp->t_segq, te, tqe_q);
	} else {
		LIST_INSERT_AFTER(p, te, tqe_q);
	}

present:
	/*
	 * Present data to user, advancing rcv_nxt through
	 * completed sequence space.
	 */
	if (!TCPS_HAVEESTABLISHED(tp->t_state)) {
		return 0;
	}
	q = LIST_FIRST(&tp->t_segq);
	if (!q || q->tqe_th->th_seq != tp->rcv_nxt) {
		return 0;
	}

	/*
	 * If there is already another thread doing reassembly for this
	 * connection, it is better to let it finish the job --
	 * (radar 16316196)
	 */
	if (tp->t_flagsext & TF_REASS_INPROG) {
		return 0;
	}

	tp->t_flagsext |= TF_REASS_INPROG;
	/* lost packet was recovered, so ooo data can be returned */
	tcpstat.tcps_recovered_pkts++;

	do {
		tp->rcv_nxt += q->tqe_len;
		flags = q->tqe_th->th_flags & TH_FIN;
		LIST_REMOVE(q, tqe_q);
		tp->t_reassq_mbcnt -= _MSIZE + (q->tqe_m->m_flags & M_EXT) ?
		    q->tqe_m->m_ext.ext_size : 0;
		if (so->so_state & SS_CANTRCVMORE) {
			m_freem(q->tqe_m);
		} else {
			so_recv_data_stat(so, q->tqe_m, 0); /* XXXX */
			if (q->tqe_th->th_flags & TH_PUSH) {
				tp->t_flagsext |= TF_LAST_IS_PSH;
			} else {
				tp->t_flagsext &= ~TF_LAST_IS_PSH;
			}

			if (sbappendstream_rcvdemux(so, q->tqe_m)) {
				*dowakeup = 1;
			}
		}
		zfree(tcp_reass_zone, q);
		tp->t_reassqlen--;
		OSDecrementAtomic(&tcp_reass_total_qlen);
		q = LIST_FIRST(&tp->t_segq);
	} while (q && q->tqe_th->th_seq == tp->rcv_nxt);
	tp->t_flagsext &= ~TF_REASS_INPROG;

	if ((inp->inp_vflag & INP_IPV6) != 0) {
		KERNEL_DEBUG(DBG_LAYER_BEG,
		    ((inp->inp_fport << 16) | inp->inp_lport),
		    (((inp->in6p_laddr.s6_addr16[0] & 0xffff) << 16) |
		    (inp->in6p_faddr.s6_addr16[0] & 0xffff)),
		    0, 0, 0);
	} else {
		KERNEL_DEBUG(DBG_LAYER_BEG,
		    ((inp->inp_fport << 16) | inp->inp_lport),
		    (((inp->inp_laddr.s_addr & 0xffff) << 16) |
		    (inp->inp_faddr.s_addr & 0xffff)),
		    0, 0, 0);
	}

	return flags;
}

/*
 * Enter fast recovery and reduce congestion window,
 * used when CE is seen or when a tail loss
 * probe recovers the last packet. Also used by RACK.
 */
void
tcp_enter_fast_recovery(struct tcpcb *tp)
{
	/*
	 * If the current tcp cc module has
	 * defined a hook for tasks to run
	 * before entering FR, call it
	 */
	if (CC_ALGO(tp)->pre_fr != NULL) {
		CC_ALGO(tp)->pre_fr(tp);
	}
	ENTER_FASTRECOVERY(tp);
	if (tp->t_flags & TF_SENTFIN) {
		tp->snd_recover = tp->snd_max - 1;
	} else {
		tp->snd_recover = tp->snd_max;
	}

	tp->t_flagsext &= ~TF_CWND_NONVALIDATED;

	tp->t_timer[TCPT_REXMT] = 0;
	tp->t_timer[TCPT_PTO] = 0;
	tp->t_rtttime = 0;
	if (tp->t_flagsext & TF_CWND_NONVALIDATED) {
		tcp_cc_adjust_nonvalidated_cwnd(tp);
	} else {
		/* No need to inflate the congestion window */
		tp->snd_cwnd = tp->snd_ssthresh;
	}
}

/*
 * This function is called upon reception of data on a socket. It's purpose is
 * to handle the adaptive keepalive timers that monitor whether the connection
 * is making progress. First the adaptive read-timer, second the TFO probe-timer.
 *
 * The application wants to get an event if there is a stall during read.
 * Set the initial keepalive timeout to be equal to twice RTO.
 *
 * If the outgoing interface is in marginal conditions, we need to
 * enable read probes for that too.
 */
static inline void
tcp_adaptive_rwtimo_check(struct tcpcb *tp, int tlen)
{
	struct ifnet *outifp = tp->t_inpcb->inp_last_outifp;

	if ((tp->t_adaptive_rtimo > 0 ||
	    (outifp != NULL &&
	    (outifp->if_eflags & IFEF_PROBE_CONNECTIVITY)))
	    && tlen > 0 &&
	    tp->t_state == TCPS_ESTABLISHED) {
		tp->t_timer[TCPT_KEEP] = OFFSET_FROM_START(tp,
		    (TCP_REXMTVAL(tp) << 1));
		tp->t_flagsext |= TF_DETECT_READSTALL;
		tp->t_rtimo_probes = 0;
	}
}

inline void
tcp_keepalive_reset(struct tcpcb *tp)
{
	tp->t_timer[TCPT_KEEP] = OFFSET_FROM_START(tp,
	    TCP_CONN_KEEPIDLE(tp));
	tp->t_flagsext &= ~(TF_DETECT_READSTALL);
	tp->t_rtimo_probes = 0;
}

void
tcp_set_finwait_timeout(struct tcpcb *tp)
{
	/*
	 * Starting the TCPT_2MSL timer is contrary to the
	 * specification, but if we don't get a FIN
	 * we'll hang forever.
	 */
	ASSERT(tp->t_state == TCPS_FIN_WAIT_2);
	ASSERT((tp->t_inpcb->inp_socket->so_state & (SS_CANTRCVMORE)) == SS_CANTRCVMORE);

	if (tcp_fin_timeout > 0 &&
	    tcp_fin_timeout < TCP_CONN_MAXIDLE(tp)) {
		tp->t_timer[TCPT_2MSL] = OFFSET_FROM_START(tp, tcp_fin_timeout);
	} else {
		tp->t_timer[TCPT_2MSL] = OFFSET_FROM_START(tp, TCP_CONN_MAXIDLE(tp));
	}
}

/*
 * TCP input routine, follows pages 65-76 of the
 * protocol specification dated September, 1981 very closely.
 */
int
tcp6_input(struct mbuf **mp, int *offp, int proto)
{
#pragma unused(proto)
	struct mbuf *m = *mp;
	uint32_t ia6_flags;
	struct ifnet *ifp = m->m_pkthdr.rcvif;

	IP6_EXTHDR_CHECK(m, *offp, sizeof(struct tcphdr), return IPPROTO_DONE);

	/* Expect 32-bit aligned data pointer on strict-align platforms */
	MBUF_STRICT_DATA_ALIGNMENT_CHECK_32(m);

	/*
	 * draft-itojun-ipv6-tcp-to-anycast
	 * better place to put this in?
	 */
	if (ip6_getdstifaddr_info(m, NULL, &ia6_flags) == 0) {
		if (ia6_flags & IN6_IFF_ANYCAST) {
			struct ip6_hdr *ip6;

			ip6 = mtod(m, struct ip6_hdr *);
			icmp6_error(m, ICMP6_DST_UNREACH,
			    ICMP6_DST_UNREACH_ADDR,
			    (int)((caddr_t)&ip6->ip6_dst - (caddr_t)ip6));

			IF_TCP_STATINC(ifp, icmp6unreach);

			return IPPROTO_DONE;
		}
	}

	tcp_input(m, *offp);
	return IPPROTO_DONE;
}

static void
tcp_sbrcv_reserve(struct tcpcb *tp, struct sockbuf *sbrcv,
    u_int32_t newsize, u_int32_t idealsize, u_int32_t rcvbuf_max)
{
	/* newsize should not exceed max */
	newsize = min(newsize, rcvbuf_max);

	/* The receive window scale negotiated at the
	 * beginning of the connection will also set a
	 * limit on the socket buffer size
	 */
	newsize = min(newsize, TCP_MAXWIN << tp->rcv_scale);

	/* Set new socket buffer size */
	if (newsize > sbrcv->sb_hiwat &&
	    (sbreserve(sbrcv, newsize) == 1)) {
		sbrcv->sb_idealsize = min(max(sbrcv->sb_idealsize,
		    (idealsize != 0) ? idealsize : newsize), rcvbuf_max);

		/* Again check the limit set by the advertised
		 * window scale
		 */
		sbrcv->sb_idealsize = min(sbrcv->sb_idealsize,
		    TCP_MAXWIN << tp->rcv_scale);
	}
}

/*
 * This function is used to grow  a receive socket buffer. It
 * will take into account system-level memory usage and the
 * bandwidth available on the link to make a decision.
 */
static void
tcp_sbrcv_grow(struct tcpcb *tp, struct sockbuf *sbrcv,
    struct tcpopt *to, uint32_t pktlen)
{
	struct socket *so = sbrcv->sb_so;

	/*
	 * Do not grow the receive socket buffer if
	 * - auto resizing is disabled, globally or on this socket
	 * - the high water mark already reached the maximum
	 * - the stream is in background and receive side is being
	 * throttled
	 */
	if (tcp_do_autorcvbuf == 0 ||
	    (sbrcv->sb_flags & SB_AUTOSIZE) == 0 ||
	    sbrcv->sb_hiwat >= tcp_autorcvbuf_max ||
	    (tp->t_flagsext & TF_RECV_THROTTLE) ||
	    (so->so_flags1 & SOF1_EXTEND_BK_IDLE_WANTED) ||
	    (!tcp_autotune_reorder && !LIST_EMPTY(&tp->t_segq))) {
		/* Can not resize the socket buffer, just return */
		goto out;
	}

	if (!TSTMP_SUPPORTED(tp)) {
		/*
		 * Timestamp option is not supported on this connection,
		 * use receiver's RTT. Socket buffer grows based on the
		 * BDP of the link.
		 */
		if (TSTMP_GEQ(tcp_now,
		    tp->rfbuf_ts + (tp->rcv_srtt >> TCP_RTT_SHIFT))) {
			tp->rfbuf_cnt += pktlen;
			if (tp->rfbuf_cnt > tp->rfbuf_space) {
				int32_t rcvbuf_inc;
				uint32_t idealsize;

				/*
				 * Increase receive-buffer aggressively if we
				 * received more than 150% of what was received
				 * in the previous round. Because, that means
				 * the sender is in TCP slow-start and so
				 * we need to give it more space to not be
				 * limiting the sender with a small receive-window.
				 */
				if (tp->rfbuf_cnt > tp->rfbuf_space + (tp->rfbuf_space >> 1)) {
					rcvbuf_inc = (tp->rfbuf_cnt << 2) - sbrcv->sb_hiwat;
					idealsize = (tp->rfbuf_cnt << 2);
				} else {
					rcvbuf_inc = (tp->rfbuf_cnt << 1) - sbrcv->sb_hiwat;
					idealsize = (tp->rfbuf_cnt << 1);
				}

				if (rcvbuf_inc > 0) {
					rcvbuf_inc =
					    (rcvbuf_inc / tp->t_maxseg) * tp->t_maxseg;

					tcp_sbrcv_reserve(tp, sbrcv,
					    sbrcv->sb_hiwat + rcvbuf_inc,
					    idealsize, tcp_autorcvbuf_max);

					tp->rfbuf_space = tp->rfbuf_cnt;
				}
			}
			goto out;
		} else {
			tp->rfbuf_cnt += pktlen;
			return;
		}
	} else if (to->to_tsecr != 0) {
		/*
		 * If the timestamp shows that one RTT has
		 * completed, we can stop counting the
		 * bytes. Here we consider increasing
		 * the socket buffer if the bandwidth measured in
		 * last rtt, is more than half of sb_hiwat, this will
		 * help to scale the buffer according to the bandwidth
		 * on the link.
		 */
		if (TSTMP_GEQ(to->to_tsecr, tp->rfbuf_ts)) {
			tp->rfbuf_cnt += pktlen;

			if (tp->rfbuf_cnt > tp->rfbuf_space) {
				int32_t rcvbuf_inc;
				uint32_t idealsize;

				if (tp->rfbuf_cnt > tp->rfbuf_space + (tp->rfbuf_space >> 1)) {
					rcvbuf_inc = (tp->rfbuf_cnt << 2) - sbrcv->sb_hiwat;
					idealsize = (tp->rfbuf_cnt << 2);
				} else {
					rcvbuf_inc = (tp->rfbuf_cnt << 1) - sbrcv->sb_hiwat;
					idealsize = (tp->rfbuf_cnt << 1);
				}

				tp->rfbuf_space = tp->rfbuf_cnt;

				if (rcvbuf_inc > 0) {
					rcvbuf_inc =
					    (rcvbuf_inc / tp->t_maxseg) * tp->t_maxseg;

					tcp_sbrcv_reserve(tp, sbrcv,
					    sbrcv->sb_hiwat + rcvbuf_inc,
					    idealsize, tcp_autorcvbuf_max);
				}
			}
			/* Measure instantaneous receive bandwidth */
			if (tp->t_bwmeas != NULL && tp->rfbuf_cnt > 0 &&
			    TSTMP_GT(tcp_now, tp->rfbuf_ts)) {
				u_int32_t rcv_bw;
				rcv_bw = tp->rfbuf_cnt /
				    (int)(tcp_now - tp->rfbuf_ts);
				if (tp->t_bwmeas->bw_rcvbw_max == 0) {
					tp->t_bwmeas->bw_rcvbw_max = rcv_bw;
				} else {
					tp->t_bwmeas->bw_rcvbw_max = max(
						tp->t_bwmeas->bw_rcvbw_max, rcv_bw);
				}
			}
			goto out;
		} else {
			tp->rfbuf_cnt += pktlen;
			return;
		}
	}
out:
	/* Restart the measurement */
	tp->rfbuf_ts = tcp_now;
	tp->rfbuf_cnt = 0;
	return;
}

/* This function will trim the excess space added to the socket buffer
 * to help a slow-reading app. The ideal-size of a socket buffer depends
 * on the link bandwidth or it is set by an application and we aim to
 * reach that size.
 */
void
tcp_sbrcv_trim(struct tcpcb *tp, struct sockbuf *sbrcv)
{
	if (tcp_do_autorcvbuf == 1 && sbrcv->sb_idealsize > 0 &&
	    sbrcv->sb_hiwat > sbrcv->sb_idealsize) {
		int32_t trim;
		/* compute the difference between ideal and current sizes */
		u_int32_t diff = sbrcv->sb_hiwat - sbrcv->sb_idealsize;

		/* Compute the maximum advertised window for
		 * this connection.
		 */
		u_int32_t advwin = tp->rcv_adv - tp->rcv_nxt;

		/* How much can we trim the receive socket buffer?
		 * 1. it can not be trimmed beyond the max rcv win advertised
		 * 2. if possible, leave 1/16 of bandwidth*delay to
		 * avoid closing the win completely
		 */
		u_int32_t leave = max(advwin, (sbrcv->sb_idealsize >> 4));

		/* Sometimes leave can be zero, in that case leave at least
		 * a few segments worth of space.
		 */
		if (leave == 0) {
			leave = tp->t_maxseg << tcp_autorcvbuf_inc_shift;
		}

		trim = sbrcv->sb_hiwat - (sbrcv->sb_cc + leave);
		trim = imin(trim, (int32_t)diff);

		if (trim > 0) {
			sbreserve(sbrcv, (sbrcv->sb_hiwat - trim));
		}
	}
}

/* We may need to trim the send socket buffer size for two reasons:
 * 1. if the rtt seen on the connection is climbing up, we do not
 * want to fill the buffers any more.
 * 2. if the congestion win on the socket backed off, there is no need
 * to hold more mbufs for that connection than what the cwnd will allow.
 */
void
tcp_sbsnd_trim(struct sockbuf *sbsnd)
{
	if (((sbsnd->sb_flags & (SB_AUTOSIZE | SB_TRIM)) ==
	    (SB_AUTOSIZE | SB_TRIM)) &&
	    (sbsnd->sb_idealsize > 0) &&
	    (sbsnd->sb_hiwat > sbsnd->sb_idealsize)) {
		u_int32_t trim = 0;
		if (sbsnd->sb_cc <= sbsnd->sb_idealsize) {
			trim = sbsnd->sb_hiwat - sbsnd->sb_idealsize;
		} else {
			trim = sbsnd->sb_hiwat - sbsnd->sb_cc;
		}
		sbreserve(sbsnd, (sbsnd->sb_hiwat - trim));
	}
	if (sbsnd->sb_hiwat <= sbsnd->sb_idealsize) {
		sbsnd->sb_flags &= ~(SB_TRIM);
	}
}

/*
 * If timestamp option was not negotiated on this connection
 * and this connection is on the receiving side of a stream
 * then we can not measure the delay on the link accurately.
 * Instead of enabling automatic receive socket buffer
 * resizing, just give more space to the receive socket buffer.
 */
static inline void
tcp_sbrcv_tstmp_check(struct tcpcb *tp)
{
	struct socket *so = tp->t_inpcb->inp_socket;
	u_int32_t newsize = 2 * tcp_recvspace;
	struct sockbuf *sbrcv = &so->so_rcv;

	if ((tp->t_flags & (TF_REQ_TSTMP | TF_RCVD_TSTMP)) !=
	    (TF_REQ_TSTMP | TF_RCVD_TSTMP) &&
	    (sbrcv->sb_flags & SB_AUTOSIZE) != 0) {
		tcp_sbrcv_reserve(tp, sbrcv, newsize, 0, newsize);
	}
}

/* A receiver will evaluate the flow of packets on a connection
 * to see if it can reduce ack traffic. The receiver will start
 * stretching acks if all of the following conditions are met:
 * 1. tcp_delack_enabled is set to 3
 * 2. If the bytes received in the last 100ms is greater than a threshold
 *      defined by maxseg_unacked
 * 3. If the connection has not been idle for tcp_maxrcvidle period.
 * 4. If the connection has seen enough packets to let the slow-start
 *      finish after connection establishment or after some packet loss.
 *
 * The receiver will stop stretching acks if there is congestion/reordering
 * as indicated by packets on reassembly queue or an ECN. If the delayed-ack
 * timer fires while stretching acks, it means that the packet flow has gone
 * below the threshold defined by maxseg_unacked and the receiver will stop
 * stretching acks. The receiver gets no indication when slow-start is completed
 * or when the connection reaches an idle state. That is why we use
 * tcp_rcvsspktcnt to cover slow-start and tcp_maxrcvidle to identify idle
 * state.
 */
static inline int
tcp_stretch_ack_enable(struct tcpcb *tp, int thflags)
{
	if (tp->rcv_by_unackwin >= (maxseg_unacked * tp->t_maxseg) &&
	    TSTMP_GEQ(tp->rcv_unackwin, tcp_now)) {
		tp->t_flags |= TF_STREAMING_ON;
	} else {
		tp->t_flags &= ~TF_STREAMING_ON;
	}

	/* If there has been an idle time, reset streaming detection */
	if (TSTMP_GT(tcp_now, tp->rcv_unackwin + tcp_maxrcvidle)) {
		tp->t_flags &= ~TF_STREAMING_ON;
	}

	/*
	 * If there are flags other than TH_ACK set, reset streaming
	 * detection
	 */
	if (thflags & ~TH_ACK) {
		tp->t_flags &= ~TF_STREAMING_ON;
	}

	if (tp->t_flagsext & TF_DISABLE_STRETCHACK) {
		if (tp->rcv_nostrack_pkts >= TCP_STRETCHACK_ENABLE_PKTCNT) {
			tp->t_flagsext &= ~TF_DISABLE_STRETCHACK;
			tp->rcv_nostrack_pkts = 0;
			tp->rcv_nostrack_ts = 0;
		} else {
			tp->rcv_nostrack_pkts++;
		}
	}

	if (!(tp->t_flagsext & (TF_NOSTRETCHACK | TF_DISABLE_STRETCHACK)) &&
	    (tp->t_flags & TF_STREAMING_ON) &&
	    (!(tp->t_flagsext & TF_RCVUNACK_WAITSS) ||
	    (tp->rcv_waitforss >= tcp_rcvsspktcnt))) {
		return 1;
	}

	return 0;
}

/*
 * Reset the state related to stretch-ack algorithm. This will make
 * the receiver generate an ack every other packet. The receiver
 * will start re-evaluating the rate at which packets come to decide
 * if it can benefit by lowering the ack traffic.
 */
void
tcp_reset_stretch_ack(struct tcpcb *tp)
{
	tp->t_flags &= ~(TF_STRETCHACK | TF_STREAMING_ON);
	tp->rcv_by_unackwin = 0;
	tp->rcv_by_unackhalfwin = 0;
	tp->rcv_unackwin = tcp_now + tcp_rcvunackwin;

	/*
	 * When there is packet loss or packet re-ordering or CWR due to
	 * ECN, the sender's congestion window is reduced. In these states,
	 * generate an ack for every other packet for some time to allow
	 * the sender's congestion window to grow.
	 */
	tp->t_flagsext |= TF_RCVUNACK_WAITSS;
	tp->rcv_waitforss = 0;
}

/*
 * The last packet was a retransmission, check if this ack
 * indicates that the retransmission was spurious.
 *
 * If the connection supports timestamps, we could use it to
 * detect if the last retransmit was not needed. Otherwise,
 * we check if the ACK arrived within RTT/2 window, then it
 * was a mistake to do the retransmit in the first place.
 *
 * This function will return 1 if it is a spurious retransmit,
 * 0 otherwise.
 */
int
tcp_detect_bad_rexmt(struct tcpcb *tp, struct tcphdr *th,
    struct tcpopt *to, u_int32_t rxtime)
{
	int32_t tdiff, bad_rexmt_win;
	bad_rexmt_win = (tp->t_srtt >> (TCP_RTT_SHIFT + 1));

	/* If the ack has ECN CE bit, then cwnd has to be adjusted */
	if ((TCP_ACC_ECN_ON(tp) && tp->t_aecn.t_delta_ce_packets > 0) ||
	    (TCP_ECN_ENABLED(tp) && (th->th_flags & TH_ECE))) {
		return 0;
	}
	if (TSTMP_SUPPORTED(tp)) {
		if (rxtime > 0 && (to->to_flags & TOF_TS) && to->to_tsecr != 0 &&
		    TSTMP_LT(to->to_tsecr, rxtime)) {
			return 1;
		}
	} else {
		if ((tp->t_rxtshift == 1 || (tp->t_flagsext & TF_SENT_TLPROBE)) &&
		    rxtime > 0) {
			tdiff = (int32_t)(tcp_now - rxtime);
			if (tdiff < bad_rexmt_win) {
				return 1;
			}
		}
	}
	return 0;
}


/*
 * Restore congestion window state if a spurious timeout
 * was detected.
 */
static void
tcp_bad_rexmt_restore_state(struct tcpcb *tp, struct tcphdr *th)
{
	if (TSTMP_SUPPORTED(tp)) {
		u_int32_t fsize, acked;
		fsize = tp->snd_max - th->th_ack;
		acked = BYTES_ACKED(th, tp);

		/*
		 * Implement bad retransmit recovery as
		 * described in RFC 4015.
		 */
		tp->snd_ssthresh = tp->snd_ssthresh_prev;

		/* Initialize cwnd to the initial window */
		if (CC_ALGO(tp)->cwnd_init != NULL) {
			CC_ALGO(tp)->cwnd_init(tp);
		}

		tp->snd_cwnd = fsize + min(acked, tp->snd_cwnd);
	} else {
		tp->snd_cwnd = tp->snd_cwnd_prev;
		tp->snd_ssthresh = tp->snd_ssthresh_prev;
		if (tp->t_flags & TF_WASFRECOVERY) {
			ENTER_FASTRECOVERY(tp);
		}

		/* Do not use the loss flight size in this case */
		tp->t_lossflightsize = 0;
	}
	tp->snd_cwnd = max(tp->snd_cwnd, tcp_initial_cwnd(tp));
	tp->snd_recover = tp->snd_recover_prev;
	tp->snd_nxt = tp->snd_max;

	/* Fix send socket buffer to reflect the change in cwnd */
	tcp_bad_rexmt_fix_sndbuf(tp);

	/* Restore rack related state */
	if (TCP_RACK_ENABLED(tp)) {
		tcp_rack_bad_rexmt_restore(tp);
	}

	/*
	 * This RTT might reflect the extra delay induced
	 * by the network. Skip using this sample for RTO
	 * calculation and mark the connection so we can
	 * recompute RTT when the next eligible sample is
	 * found.
	 */
	tp->t_flagsext |= TF_RECOMPUTE_RTT;
	tp->t_badrexmt_time = tcp_now;
	tp->t_rtttime = 0;
}

/*
 * If the previous packet was sent in retransmission timer, and it was
 * not needed, then restore the congestion window to the state before that
 * transmission.
 *
 * If the last packet was sent in tail loss probe timeout, check if that
 * recovered the last packet. If so, that will indicate a real loss and
 * the congestion window needs to be lowered.
 */
static void
tcp_bad_rexmt_check(struct tcpcb *tp, struct tcphdr *th, struct tcpopt *to)
{
	if (tp->t_rxtshift > 0 &&
	    tcp_detect_bad_rexmt(tp, th, to, tp->t_rxtstart)) {
		++tcpstat.tcps_sndrexmitbad;
		tcp_bad_rexmt_restore_state(tp, th);
		tcp_ccdbg_trace(tp, th, TCP_CC_BAD_REXMT_RECOVERY);
	} else if ((tp->t_flagsext & TF_SENT_TLPROBE) && tp->t_tlphighrxt > 0 &&
	    SEQ_GEQ(th->th_ack, tp->t_tlphighrxt) &&
	    !tcp_detect_bad_rexmt(tp, th, to, tp->t_tlpstart)) {
		/*
		 * The tail loss probe recovered the last packet and
		 * we need to adjust the congestion window to take
		 * this loss into account.
		 * No need to update rack.reo_wnd_persist for a TLP recovery
		 */
		++tcpstat.tcps_tlp_recoverlastpkt;
		if (!IN_FASTRECOVERY(tp)) {
			tcp_enter_fast_recovery(tp);
			EXIT_FASTRECOVERY(tp);
		}
		tcp_ccdbg_trace(tp, th, TCP_CC_TLP_RECOVER_LASTPACKET);
	} else if (tcp_rxtseg_detect_bad_rexmt(tp, th->th_ack)) {
		/*
		 * All of the retransmitted segments were duplicated, this
		 * can be an indication of bad fast retransmit.
		 */
		tcpstat.tcps_dsack_badrexmt++;
		tcp_bad_rexmt_restore_state(tp, th);
		tcp_ccdbg_trace(tp, th, TCP_CC_DSACK_BAD_REXMT);
		tcp_rxtseg_clean(tp);
	}
	tp->t_flagsext &= ~(TF_SENT_TLPROBE);
	tp->t_tlphighrxt = 0;
	tp->t_tlpstart = 0;

	/*
	 * check if the latest ack was for a segment sent during PMTU
	 * blackhole detection. If the timestamp on the ack is before
	 * PMTU blackhole detection, then revert the size of the max
	 * segment to previous size.
	 */
	if (tp->t_rxtshift > 0 && (tp->t_flags & TF_BLACKHOLE) &&
	    tp->t_pmtud_start_ts > 0 && TSTMP_SUPPORTED(tp)) {
		if ((to->to_flags & TOF_TS) && to->to_tsecr != 0
		    && TSTMP_LT(to->to_tsecr, tp->t_pmtud_start_ts)) {
			tcp_pmtud_revert_segment_size(tp);
		}
	}
	if (tp->t_pmtud_start_ts > 0) {
		tp->t_pmtud_start_ts = 0;
	}

	tp->t_pmtud_lastseg_size = 0;
}

/*
 * Check if early retransmit can be attempted according to RFC 5827.
 *
 * If packet reordering is detected on a connection, fast recovery will
 * be delayed until it is clear that the packet was lost and not reordered.
 * But reordering detection is done only when SACK is enabled.
 *
 * On connections that do not support SACK, there is a limit on the number
 * of early retransmits that can be done per minute. This limit is needed
 * to make sure that too many packets are not retransmitted when there is
 * packet reordering.
 */
static void
tcp_early_rexmt_check(struct tcpcb *tp, struct tcphdr *th)
{
	u_int32_t obytes, snd_off;
	int32_t snd_len;
	struct socket *so = tp->t_inpcb->inp_socket;

	if ((SACK_ENABLED(tp) || tp->t_early_rexmt_count < TCP_EARLY_REXMT_LIMIT) &&
	    SEQ_GT(tp->snd_max, tp->snd_una) &&
	    (tp->t_dupacks == 1 || (SACK_ENABLED(tp) && !TAILQ_EMPTY(&tp->snd_holes)))) {
		/*
		 * If there are only a few outstanding
		 * segments on the connection, we might need
		 * to lower the retransmit threshold. This
		 * will allow us to do Early Retransmit as
		 * described in RFC 5827.
		 */
		if (TCP_RACK_ENABLED(tp)) {
			obytes = tcp_flight_size(tp);
		} else if (SACK_ENABLED(tp) &&
		    !TAILQ_EMPTY(&tp->snd_holes)) {
			obytes = tcp_flight_size(tp);
		} else {
			obytes = (tp->snd_max - tp->snd_una);
		}

		/*
		 * In order to lower retransmit threshold the
		 * following two conditions must be met.
		 * 1. the amount of outstanding data is less
		 * than 4*SMSS bytes
		 * 2. there is no unsent data ready for
		 * transmission or the advertised window
		 * will limit sending new segments.
		 */
		snd_off = tp->snd_max - tp->snd_una;
		snd_len = min(so->so_snd.sb_cc, tp->snd_wnd) - snd_off;
		if (obytes < (tp->t_maxseg << 2) &&
		    snd_len <= 0) {
			u_int32_t osegs;

			osegs = obytes / tp->t_maxseg;
			if ((osegs * tp->t_maxseg) < obytes) {
				osegs++;
			}

			/*
			 * By checking for early retransmit after
			 * receiving some duplicate acks when SACK
			 * is supported, the connection will
			 * enter fast recovery even if multiple
			 * segments are lost in the same window.
			 */
			if (osegs < 4) {
				tp->t_rexmtthresh =
				    ((osegs - 1) > 1) ? ((uint8_t)osegs - 1) : 1;
				tp->t_rexmtthresh =
				    MIN(tp->t_rexmtthresh, tcprexmtthresh);
				tp->t_rexmtthresh =
				    MAX(tp->t_rexmtthresh,
				    tp->t_dupacks > UINT8_MAX ? UINT8_MAX : (uint8_t)tp->t_dupacks);

				if (tp->t_early_rexmt_count == 0) {
					tp->t_early_rexmt_win = tcp_now;
				}

				if (tp->t_flagsext & TF_SENT_TLPROBE) {
					tcpstat.tcps_tlp_recovery++;
					tcp_ccdbg_trace(tp, th,
					    TCP_CC_TLP_RECOVERY);
				} else {
					tcpstat.tcps_early_rexmt++;
					tp->t_early_rexmt_count++;
					tcp_ccdbg_trace(tp, th,
					    TCP_CC_EARLY_RETRANSMIT);
				}
			}
		}
	}

	/*
	 * If we ever sent a TLP probe, the acknowledgement will trigger
	 * early retransmit because the value of snd_fack will be close
	 * to snd_max. This will take care of adjustments to the
	 * congestion window. So we can reset TF_SENT_PROBE flag.
	 */
	tp->t_flagsext &= ~(TF_SENT_TLPROBE);
	tp->t_tlphighrxt = 0;
	tp->t_tlpstart = 0;
}

static boolean_t
tcp_tfo_syn(struct tcpcb *tp, struct tcpopt *to)
{
	u_char out[CCAES_BLOCK_SIZE];
	unsigned char len;

	if (!(to->to_flags & (TOF_TFO | TOF_TFOREQ)) ||
	    !(tcp_fastopen & TCP_FASTOPEN_SERVER)) {
		return FALSE;
	}

	if ((to->to_flags & TOF_TFOREQ)) {
		tp->t_tfo_flags |= TFO_F_OFFER_COOKIE;

		tp->t_tfo_stats |= TFO_S_COOKIEREQ_RECV;
		tcpstat.tcps_tfo_cookie_req_rcv++;
		return FALSE;
	}

	/* Ok, then it must be an offered cookie. We need to check that ... */
	tcp_tfo_gen_cookie(tp->t_inpcb, out, sizeof(out));

	len = *to->to_tfo - TCPOLEN_FASTOPEN_REQ;
	to->to_tfo++;
	if (memcmp(out, to->to_tfo, len)) {
		/* Cookies are different! Let's return and offer a new cookie */
		tp->t_tfo_flags |= TFO_F_OFFER_COOKIE;

		tp->t_tfo_stats |= TFO_S_COOKIE_INVALID;
		tcpstat.tcps_tfo_cookie_invalid++;
		return FALSE;
	}

	if (OSIncrementAtomic(&tcp_tfo_halfcnt) >= tcp_tfo_backlog) {
		/* Need to decrement again as we just increased it... */
		OSDecrementAtomic(&tcp_tfo_halfcnt);
		return FALSE;
	}

	tp->t_tfo_flags |= TFO_F_COOKIE_VALID;

	tp->t_tfo_stats |= TFO_S_SYNDATA_RCV;
	tcpstat.tcps_tfo_syn_data_rcv++;

	return TRUE;
}

static void
tcp_tfo_synack(struct tcpcb *tp, struct tcpopt *to)
{
	if (to->to_flags & TOF_TFO) {
		unsigned char len = *to->to_tfo - TCPOLEN_FASTOPEN_REQ;

		/*
		 * If this happens, things have gone terribly wrong. len should
		 * have been checked in tcp_dooptions.
		 */
		VERIFY(len <= TFO_COOKIE_LEN_MAX);

		to->to_tfo++;

		tcp_cache_set_cookie(tp, to->to_tfo, len);
		tcp_heuristic_tfo_success(tp);

		tp->t_tfo_stats |= TFO_S_COOKIE_RCV;
		tcpstat.tcps_tfo_cookie_rcv++;
		if (tp->t_tfo_flags & TFO_F_COOKIE_SENT) {
			tcpstat.tcps_tfo_cookie_wrong++;
			tp->t_tfo_stats |= TFO_S_COOKIE_WRONG;
		}
	} else {
		/*
		 * Thus, no cookie in the response, but we either asked for one
		 * or sent SYN+DATA. Now, we need to check whether we had to
		 * rexmit the SYN. If that's the case, it's better to start
		 * backing of TFO-cookie requests.
		 */
		if (!(tp->t_flagsext & TF_FASTOPEN_FORCE_ENABLE) &&
		    tp->t_tfo_flags & TFO_F_SYN_LOSS) {
			tp->t_tfo_stats |= TFO_S_SYN_LOSS;
			tcpstat.tcps_tfo_syn_loss++;

			tcp_heuristic_tfo_loss(tp);
		} else {
			if (tp->t_tfo_flags & TFO_F_COOKIE_REQ) {
				tp->t_tfo_stats |= TFO_S_NO_COOKIE_RCV;
				tcpstat.tcps_tfo_no_cookie_rcv++;
			}

			tcp_heuristic_tfo_success(tp);
		}
	}
}

static void
tcp_tfo_rcv_probe(struct tcpcb *tp, int tlen)
{
	if (tlen != 0) {
		return;
	}

	tp->t_tfo_probe_state = TFO_PROBE_PROBING;

	/*
	 * We send the probe out rather quickly (after one RTO). It does not
	 * really hurt that much, it's only one additional segment on the wire.
	 */
	tp->t_timer[TCPT_KEEP] = OFFSET_FROM_START(tp, (TCP_REXMTVAL(tp)));
}

static void
tcp_tfo_rcv_data(struct tcpcb *tp)
{
	/* Transition from PROBING to NONE as data has been received */
	if (tp->t_tfo_probe_state >= TFO_PROBE_PROBING) {
		tp->t_tfo_probe_state = TFO_PROBE_NONE;
	}
}

static void
tcp_tfo_rcv_ack(struct tcpcb *tp, struct tcphdr *th)
{
	if (tp->t_tfo_probe_state == TFO_PROBE_PROBING &&
	    tp->t_tfo_probes > 0) {
		if (th->th_seq == tp->rcv_nxt) {
			/* No hole, so stop probing */
			tp->t_tfo_probe_state = TFO_PROBE_NONE;
		} else if (SEQ_GT(th->th_seq, tp->rcv_nxt)) {
			/* There is a hole! Wait a bit for data... */
			tp->t_tfo_probe_state = TFO_PROBE_WAIT_DATA;
			tp->t_timer[TCPT_KEEP] = OFFSET_FROM_START(tp,
			    TCP_REXMTVAL(tp));
		}
	}
}

/*
 * Update snd_wnd information.
 */
static inline bool
tcp_update_window(struct tcpcb *tp, int thflags, struct tcphdr * th,
    u_int32_t tiwin, int tlen)
{
	/* Don't look at the window if there is no ACK flag */
	if ((thflags & TH_ACK) &&
	    (SEQ_LT(tp->snd_wl1, th->th_seq) ||
	    (tp->snd_wl1 == th->th_seq && (SEQ_LT(tp->snd_wl2, th->th_ack) ||
	    (tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd))))) {
		/* keep track of pure window updates */
		if (tlen == 0 &&
		    tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd) {
			tcpstat.tcps_rcvwinupd++;
		}
		tp->snd_wnd = tiwin;
		tp->snd_wl1 = th->th_seq;
		tp->snd_wl2 = th->th_ack;
		if (tp->snd_wnd > tp->max_sndwnd) {
			tp->max_sndwnd = tp->snd_wnd;
		}

		if (tp->t_inpcb->inp_socket->so_flags & SOF_MP_SUBFLOW) {
			mptcp_update_window_wakeup(tp);
		}
		return true;
	}
	return false;
}

static void
tcp_handle_wakeup(struct socket *so, int read_wakeup, int write_wakeup)
{
	if (read_wakeup != 0) {
		sorwakeup(so);
	}
	if (write_wakeup != 0) {
		sowwakeup(so);
	}
}

static void
tcp_update_snd_una(struct tcpcb *tp, uint32_t ack)
{
	tp->snd_una = ack;
}

static bool
tcp_syn_data_valid(struct tcpcb *tp, struct tcphdr *tcp_hdr, int tlen)
{
	/* No data? */
	if (tlen <= 0) {
		return false;
	}

	/* Not the right sequence-number? */
	if (tcp_hdr->th_seq != tp->irs) {
		return false;
	}

	/* We could have wrapped around, check that */
	if (tp->t_inpcb->inp_stat->rxbytes > INT32_MAX) {
		return false;
	}

	return true;
}

/* Process IP-ECN codepoints on received packets and update receive side counters */
static void
tcp_input_ip_ecn(struct tcpcb *tp, struct inpcb *inp, uint32_t tlen,
    uint32_t segment_count, uint8_t ip_ecn)
{
	switch (ip_ecn) {
	case IPTOS_ECN_ECT1:
		tp->ecn_flags |= TE_ACO_ECT1;
		tp->t_aecn.t_rcv_ect1_bytes += tlen;
		break;
	case IPTOS_ECN_ECT0:
		tp->ecn_flags |= TE_ACO_ECT0;
		tp->t_aecn.t_rcv_ect0_bytes += tlen;
		break;
	case IPTOS_ECN_CE:
		tp->t_aecn.t_rcv_ce_packets += segment_count;
		tp->t_aecn.t_rcv_ce_bytes += tlen;
		tp->t_ecn_recv_ce++;
		tcpstat.tcps_ecn_recv_ce++;
		INP_INC_IFNET_STAT(inp, ecn_recv_ce);
		break;
	default:
		/* No counter for Not-ECT */
		break;
	}
}

/* Process SYN packet that wishes to negotiate Accurate ECN */
static void
tcp_input_process_accecn_syn(struct tcpcb *tp, int ace_flags, uint8_t ip_ecn)
{
	switch (ace_flags) {
	case (0 | 0 | 0):
		/* No ECN */
		tp->t_server_accecn_state = tcp_connection_server_no_ecn_requested;
		break;
	case (0 | TH_CWR | TH_ECE):
		/* Legacy ECN-setup */
		tp->ecn_flags |= (TE_SETUPRECEIVED | TE_SENDIPECT);
		tp->t_server_accecn_state = tcp_connection_server_classic_ecn_requested;
		break;
	case (TH_ACE):
		/* Accurate ECN */
		if (TCP_ACC_ECN_ENABLED(tp)) {
			switch (ip_ecn) {
			case IPTOS_ECN_NOTECT:
				tp->ecn_flags |= TE_ACE_SETUP_NON_ECT;
				break;
			case IPTOS_ECN_ECT1:
				tp->ecn_flags |= TE_ACE_SETUP_ECT1;
				break;
			case IPTOS_ECN_ECT0:
				tp->ecn_flags |= TE_ACE_SETUP_ECT0;
				break;
			case IPTOS_ECN_CE:
				tp->ecn_flags |= TE_ACE_SETUP_CE;
				break;
			}
			/*
			 * We set TE_SENDIPECT when handshake is complete
			 * for Accurate ECN
			 */
			tp->ecn_flags |= (TE_ACE_SETUPRECEIVED);

			/* Initialize ECT byte counter to 1 to distinguish zeroing of options */
			tp->t_aecn.t_rcv_ect1_bytes = tp->t_aecn.t_rcv_ect0_bytes = 1;
			tp->t_aecn.t_snd_ect1_bytes = tp->t_aecn.t_snd_ect0_bytes = 1;
			tp->t_server_accecn_state = tcp_connection_server_accurate_ecn_requested;
		} else {
			/*
			 * If AccECN is not enabled, ignore
			 * the TH_AE bit and do Legacy ECN-setup
			 */
			tp->ecn_flags |= (TE_SETUPRECEIVED | TE_SENDIPECT);
		}
	default:
		/* Forward Compatibility */
		/* Accurate ECN */
		if (TCP_ACC_ECN_ENABLED(tp)) {
			switch (ip_ecn) {
			case IPTOS_ECN_NOTECT:
				tp->ecn_flags |= TE_ACE_SETUP_NON_ECT;
				break;
			case IPTOS_ECN_ECT1:
				tp->ecn_flags |= TE_ACE_SETUP_ECT1;
				break;
			case IPTOS_ECN_ECT0:
				tp->ecn_flags |= TE_ACE_SETUP_ECT0;
				break;
			case IPTOS_ECN_CE:
				tp->ecn_flags |= TE_ACE_SETUP_CE;
				break;
			}
			/*
			 * We are not yet committing to send IP ECT packets when
			 * Accurate ECN is enabled
			 */
			tp->ecn_flags |= (TE_ACE_SETUPRECEIVED);

			/* Initialize ECT byte counter to 1 to distinguish zeroing of options */
			tp->t_aecn.t_rcv_ect1_bytes = tp->t_aecn.t_rcv_ect0_bytes = 1;
			tp->t_aecn.t_snd_ect1_bytes = tp->t_aecn.t_snd_ect0_bytes = 1;
			tp->t_server_accecn_state = tcp_connection_server_accurate_ecn_requested;
		}
		break;
	}
}

static uint32_t
tcp_process_ace_field(struct tcpcb *tp, uint32_t pkts_acked, uint64_t old_sceb, uint8_t ace)
{
	/* Congestion was experienced if delta_cep > 0 */
	uint32_t delta = 0, safe_delta = 0;
	delta = (ace + TCP_ACE_DIV -
	    (tp->t_aecn.t_snd_ce_packets & TCP_ACE_MASK)) & TCP_ACE_MASK;
	if (pkts_acked <= TCP_ACE_MASK) {
		return delta;
	}

	uint64_t d_ceb = tp->t_aecn.t_snd_ce_bytes - old_sceb;
	safe_delta = pkts_acked - ((pkts_acked - delta) & TCP_ACE_MASK);

	if (d_ceb == 0 || d_ceb < safe_delta * tp->t_maxseg >> 1) {
		return delta;
	}

	return safe_delta;
}

/* Returns the number of CE marked bytes */
static uint32_t
tcp_process_accecn_options(struct tcpcb *tp, struct tcpopt *to)
{
	int delta = 0;
	uint32_t ce_bytes = 0;

	if (to->to_num_accecn >= 1) {
		delta = ntoh24(to->to_accecn + 0);
		if (to->to_accecn_order == 0) {
			delta = (delta + TCP_ACO_DIV -
			    (tp->t_aecn.t_snd_ect0_bytes & TCP_ACO_MASK)) & TCP_ACO_MASK;
			if (delta < 0) {
				os_log_error(OS_LOG_DEFAULT, "delta for AccECN0 options (ECT0 bytes) can't be zero");
			}
			tp->t_aecn.t_snd_ect0_bytes += delta;
		} else {
			delta = (delta + TCP_ACO_DIV -
			    (tp->t_aecn.t_snd_ect1_bytes & TCP_ACO_MASK)) & TCP_ACO_MASK;
			if (delta < 0) {
				os_log_error(OS_LOG_DEFAULT, "delta for AccECN1 options (ECT1 bytes) can't be zero");
			}
			tp->t_aecn.t_snd_ect1_bytes += delta;
		}
	}
	if (to->to_num_accecn >= 2) {
		delta = ntoh24(to->to_accecn + 1 * TCPOLEN_ACCECN_COUNTER);
		delta = (delta + TCP_ACO_DIV -
		    (tp->t_aecn.t_snd_ce_bytes & TCP_ACO_MASK)) & TCP_ACO_MASK;
		if (delta < 0) {
			os_log_error(OS_LOG_DEFAULT, "delta for AccECN options (CE bytes) can't be zero");
		}
		tp->t_aecn.t_snd_ce_bytes += delta;
		ce_bytes = delta;
	}
	if (to->to_num_accecn >= 3) {
		delta = ntoh24(to->to_accecn + 2 * TCPOLEN_ACCECN_COUNTER);
		if (to->to_accecn_order == 0) {
			delta = (delta + TCP_ACO_DIV -
			    (tp->t_aecn.t_snd_ect1_bytes & TCP_ACO_MASK)) & TCP_ACO_MASK;
			if (delta < 0) {
				os_log_error(OS_LOG_DEFAULT, "delta for AccECN0 options (ECT1 bytes) can't be zero");
			}
			tp->t_aecn.t_snd_ect1_bytes += delta;
		} else {
			delta = (delta + TCP_ACO_DIV -
			    (tp->t_aecn.t_snd_ect0_bytes & TCP_ACO_MASK)) & TCP_ACO_MASK;
			if (delta < 0) {
				os_log_error(OS_LOG_DEFAULT, "delta for AccECN1 options (ECT0 bytes) can't be zero");
			}
			tp->t_aecn.t_snd_ect0_bytes += delta;
		}
	}

	return ce_bytes;
}

static void
tcp_process_accecn(struct tcpcb *tp, struct tcpopt *to, struct tcphdr *th,
    uint32_t pkts_acked, uint8_t ace)
{
	if (tp->t_aecn.accecn_processed) {
		os_log(OS_LOG_DEFAULT, "already processed AccECN field/options for this ACK");
		return;
	}

	uint64_t old_sceb = tp->t_aecn.t_snd_ce_bytes;
	uint32_t new_ce_bytes = tcp_process_accecn_options(tp, to);
	uint32_t delta = tcp_process_ace_field(tp, pkts_acked, old_sceb, ace);
	tp->t_aecn.t_snd_ce_packets += delta;
	tp->t_aecn.t_delta_ce_packets = delta;

	/* Update the time for this newly acked data or control packet */
	if ((to->to_flags & TOF_TS) != 0 && (to->to_tsecr != 0) &&
	    TSTMP_GEQ(to->to_tsecr, tp->t_last_ack_tsecr)) {
		tp->t_last_ack_tsecr = to->to_tsecr;
	}

	if (delta > 0) {
		tp->ecn_flags |= (TE_INRECOVERY);
		tp->total_ect_packets_marked += delta;

		/* update the stats */
		tcpstat.tcps_ecn_ace_recv_ce += tp->t_aecn.t_delta_ce_packets;
		/* CE packets counter start at 5 */
		tp->t_ecn_capable_packets_marked = tp->t_aecn.t_snd_ce_packets - 5;
		tcp_ccdbg_trace(tp, th, TCP_CC_ECN_RCVD);
	}

	if (CC_ALGO(tp)->process_ecn != NULL) {
		CC_ALGO(tp)->process_ecn(tp, th, new_ce_bytes, tp->total_ect_packets_marked,
		    tp->total_ect_packets_acked);
	}

	tp->t_aecn.accecn_processed = 1;
}

void
tcp_input(struct mbuf *m, int off0)
{
	int exiting_fr = 0;
	struct tcphdr *th;
	struct ip *ip = NULL;
	struct inpcb *inp;
	u_char *optp = NULL;
	int optlen = 0;
	int tlen, off;
	int drop_hdrlen;
	struct tcpcb *tp = 0;
	int thflags;
	struct socket *so = 0;
	int todrop, acked = 0, ourfinisacked, needoutput = 0;
	int read_wakeup = 0;
	int write_wakeup = 0;
	struct in_addr laddr;
	struct in6_addr laddr6;
	int dropsocket = 0;
	int iss = 0, nosock = 0;
	uint32_t tiwin, sack_bytes_acked = 0;
	uint32_t highest_sacked_seq = 0;
	struct tcpopt to;               /* options in this segment */
	u_char ip_ecn = IPTOS_ECN_NOTECT;
	unsigned int ifscope;
	uint8_t isconnected, isdisconnected;
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	int segment_count = m->m_pkthdr.seg_cnt ? : 1;
	int win;
	u_int16_t pf_tag = 0;
#if MPTCP
	struct mptcb *mp_tp = NULL;
#endif /* MPTCP */
	stats_functional_type ifnet_count_type = IFNET_COUNT_TYPE(ifp);
	boolean_t recvd_dsack = FALSE;
	boolean_t dsack_tlp = false;
	struct tcp_respond_args tra;
	int prev_t_state;
	boolean_t check_cfil = cfil_filter_present();
	bool findpcb_iterated = false;
	bool rack_loss_detected = false;
	bool is_th_swapped = false;
	/*
	 * The mbuf may be freed after it has been added to the receive socket
	 * buffer or the reassembly queue, so we reinitialize th to point to a
	 * safe copy of the TCP header
	 */
	struct tcphdr saved_tcphdr = {};
	/*
	 * Save copy of the IPv4/IPv6 header.
	 * Note: use array of uint32_t to silence compiler warning when casting
	 * to a struct ip6_hdr pointer.
	 */
#define MAX_IPWORDS ((sizeof(struct ip) + MAX_IPOPTLEN) / sizeof(uint32_t))
	uint32_t saved_hdr[MAX_IPWORDS];

#define TCP_INC_VAR(stat, npkts) do {                   \
	        stat += npkts;                          \
} while (0)
	drop_reason_t drop_reason = DROP_REASON_UNSPECIFIED;

	if (tcp_ack_strategy == TCP_ACK_STRATEGY_LEGACY) {
		segment_count = 1;
	}
	TCP_INC_VAR(tcpstat.tcps_rcvtotal, segment_count);

	struct ip6_hdr *ip6 = NULL;
	int isipv6;
	struct proc *kernel_proc = current_proc();

	KERNEL_DEBUG(DBG_FNC_TCP_INPUT | DBG_FUNC_START, 0, 0, 0, 0, 0);

	isipv6 = (mtod(m, struct ip *)->ip_v == 6) ? 1 : 0;
	bzero((char *)&to, sizeof(to));

	m_add_crumb(m, PKT_CRUMB_TCP_INPUT);

	if (m->m_flags & M_PKTHDR) {
		pf_tag = m_pftag(m)->pftag_tag;
	}

	if (isipv6) {
		/*
		 * Expect 32-bit aligned data pointer on
		 * strict-align platforms
		 */
		MBUF_STRICT_DATA_ALIGNMENT_CHECK_32(m);

		/* IP6_EXTHDR_CHECK() is already done at tcp6_input() */
		ip6 = mtod(m, struct ip6_hdr *);
		tlen = sizeof(*ip6) + ntohs(ip6->ip6_plen) - off0;
		th = (struct tcphdr *)(void *)((caddr_t)ip6 + off0);

		if (tcp_input_checksum(AF_INET6, m, th, off0, tlen)) {
			TCP_LOG_DROP_PKT(ip6, th, ifp, "IPv6 bad tcp checksum");
			goto dropnosock;
		}

		KERNEL_DEBUG(DBG_LAYER_BEG, ((th->th_dport << 16) | th->th_sport),
		    (((ip6->ip6_src.s6_addr16[0]) << 16) | (ip6->ip6_dst.s6_addr16[0])),
		    th->th_seq, th->th_ack, th->th_win);
		/*
		 * Be proactive about unspecified IPv6 address in source.
		 * As we use all-zero to indicate unbounded/unconnected pcb,
		 * unspecified IPv6 address can be used to confuse us.
		 *
		 * Note that packets with unspecified IPv6 destination is
		 * already dropped in ip6_input.
		 */
		if (IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src)) {
			/* XXX stat */
			IF_TCP_STATINC(ifp, unspecv6);
			TCP_LOG_DROP_PKT(ip6, th, ifp, "src IPv6 address unspecified");
			goto dropnosock;
		}
		DTRACE_TCP5(receive, struct mbuf *, m, struct inpcb *, NULL,
		    struct ip6_hdr *, ip6, struct tcpcb *, NULL,
		    struct tcphdr *, th);

		ip_ecn = (ntohl(ip6->ip6_flow) >> 20) & IPTOS_ECN_MASK;
	} else {
		/*
		 * Get IP and TCP header together in first mbuf.
		 * Note: IP leaves IP header in first mbuf.
		 */
		if (off0 > sizeof(struct ip)) {
			ip_stripoptions(m);
			off0 = sizeof(struct ip);
		}
		if (m->m_len < sizeof(struct tcpiphdr)) {
			if ((m = m_pullup(m, sizeof(struct tcpiphdr))) == 0) {
				tcpstat.tcps_rcvshort++;
				return;
			}
		}

		/* Expect 32-bit aligned data pointer on strict-align platforms */
		MBUF_STRICT_DATA_ALIGNMENT_CHECK_32(m);

		ip = mtod(m, struct ip *);
		th = (struct tcphdr *)(void *)((caddr_t)ip + off0);
		tlen = ip->ip_len;

		if (tcp_input_checksum(AF_INET, m, th, off0, tlen)) {
			TCP_LOG_DROP_PKT(ip, th, ifp, "IPv4 bad tcp checksum");
			goto dropnosock;
		}

		/* Re-initialization for later version check */
		ip->ip_v = IPVERSION;
		ip_ecn = (ip->ip_tos & IPTOS_ECN_MASK);

		DTRACE_TCP5(receive, struct mbuf *, m, struct inpcb *, NULL,
		    struct ip *, ip, struct tcpcb *, NULL, struct tcphdr *, th);

		KERNEL_DEBUG(DBG_LAYER_BEG, ((th->th_dport << 16) | th->th_sport),
		    (((ip->ip_src.s_addr & 0xffff) << 16) | (ip->ip_dst.s_addr & 0xffff)),
		    th->th_seq, th->th_ack, th->th_win);
	}

#define TCP_LOG_HDR (isipv6 ? (void *)ip6 : (void *)ip)

	/*
	 * Check that TCP offset makes sense,
	 * pull out TCP options and adjust length.
	 */
	off = th->th_off << 2;
	if (off < sizeof(struct tcphdr) || off > tlen) {
		tcpstat.tcps_rcvbadoff++;
		IF_TCP_STATINC(ifp, badformat);
		TCP_LOG_DROP_PKT(TCP_LOG_HDR, th, ifp, "bad tcp offset");
		goto dropnosock;
	}
	tlen -= off;    /* tlen is used instead of ti->ti_len */
	if (off > sizeof(struct tcphdr)) {
		if (isipv6) {
			IP6_EXTHDR_CHECK(m, off0, off, return );
			ip6 = mtod(m, struct ip6_hdr *);
			th = (struct tcphdr *)(void *)((caddr_t)ip6 + off0);
		} else {
			if (m->m_len < sizeof(struct ip) + off) {
				if ((m = m_pullup(m, sizeof(struct ip) + off)) == 0) {
					tcpstat.tcps_rcvshort++;
					return;
				}
				ip = mtod(m, struct ip *);
				th = (struct tcphdr *)(void *)((caddr_t)ip + off0);
			}
		}
		optlen = off - sizeof(struct tcphdr);
		optp = (u_char *)(th + 1);
		/*
		 * Do quick retrieval of timestamp options ("options
		 * prediction?").  If timestamp is the only option and it's
		 * formatted as recommended in RFC 1323 appendix A, we
		 * quickly get the values now and not bother calling
		 * tcp_dooptions(), etc.
		 */
		if ((optlen == TCPOLEN_TSTAMP_APPA ||
		    (optlen > TCPOLEN_TSTAMP_APPA &&
		    optp[TCPOLEN_TSTAMP_APPA] == TCPOPT_EOL)) &&
		    *(u_int32_t *)(void *)optp == htonl(TCPOPT_TSTAMP_HDR) &&
		    (th->th_flags & TH_SYN) == 0) {
			to.to_flags |= TOF_TS;
			to.to_tsval = ntohl(*(u_int32_t *)(void *)(optp + 4));
			to.to_tsecr = ntohl(*(u_int32_t *)(void *)(optp + 8));
			optp = NULL;    /* we've parsed the options */
		}
	}
	thflags = th->th_flags;

	/*
	 * Drop all packets with both the SYN and FIN bits set.
	 * This prevents e.g. nmap from identifying the TCP/IP stack.
	 *
	 * This is a violation of the TCP specification.
	 */
	if ((thflags & (TH_SYN | TH_FIN)) == (TH_SYN | TH_FIN)) {
		IF_TCP_STATINC(ifp, synfin);
		TCP_LOG_DROP_PKT(TCP_LOG_HDR, th, ifp, "drop SYN FIN");
		goto dropnosock;
	}

	/*
	 * Delay dropping TCP, IP headers, IPv6 ext headers, and TCP options,
	 * until after ip6_savecontrol() is called and before other functions
	 * which don't want those proto headers.
	 * Because ip6_savecontrol() is going to parse the mbuf to
	 * search for data to be passed up to user-land, it wants mbuf
	 * parameters to be unchanged.
	 */
	drop_hdrlen = off0 + off;

	/* Since this is an entry point for input processing of tcp packets, we
	 * can update the tcp clock here.
	 */
	calculate_tcp_clock();

	/*
	 * Record the interface where this segment arrived on; this does not
	 * affect normal data output (for non-detached TCP) as it provides a
	 * hint about which route and interface to use for sending in the
	 * absence of a PCB, when scoped routing (and thus source interface
	 * selection) are enabled.
	 */
	if ((m->m_pkthdr.pkt_flags & PKTF_LOOP) || m->m_pkthdr.rcvif == NULL) {
		ifscope = IFSCOPE_NONE;
	} else {
		ifscope = m->m_pkthdr.rcvif->if_index;
	}

	/*
	 * Convert TCP protocol specific fields to host format.
	 */

#if BYTE_ORDER != BIG_ENDIAN
	NTOHL(th->th_seq);
	NTOHL(th->th_ack);
	NTOHS(th->th_win);
	NTOHS(th->th_urp);
	is_th_swapped = true;
#endif

	/*
	 * Locate pcb for segment.
	 */
findpcb:

	isconnected = FALSE;
	isdisconnected = FALSE;

	if (isipv6) {
		inp = in6_pcblookup_hash(&tcbinfo, &ip6->ip6_src, th->th_sport, ip6_input_getsrcifscope(m),
		    &ip6->ip6_dst, th->th_dport, ip6_input_getdstifscope(m), 1,
		    m->m_pkthdr.rcvif);
	} else {
		inp = in_pcblookup_hash(&tcbinfo, ip->ip_src, th->th_sport,
		    ip->ip_dst, th->th_dport, 1, m->m_pkthdr.rcvif);
	}

	/*
	 * Use the interface scope information from the PCB for outbound
	 * segments.  If the PCB isn't present and if scoped routing is
	 * enabled, tcp_respond will use the scope of the interface where
	 * the segment arrived on.
	 */
	if (inp != NULL && (inp->inp_flags & INP_BOUND_IF)) {
		ifscope = inp->inp_boundifp->if_index;
	}

	/*
	 * If the state is CLOSED (i.e., TCB does not exist) then
	 * all data in the incoming segment is discarded.
	 * If the TCB exists but is in CLOSED state, it is embryonic,
	 * but should either do a listen or a connect soon.
	 */
	if (inp == NULL) {
		if (log_in_vain) {
			char dbuf[MAX_IPv6_STR_LEN], sbuf[MAX_IPv6_STR_LEN];

			if (isipv6) {
				inet_ntop(AF_INET6, &ip6->ip6_dst, dbuf, sizeof(dbuf));
				inet_ntop(AF_INET6, &ip6->ip6_src, sbuf, sizeof(sbuf));
			} else {
				inet_ntop(AF_INET, &ip->ip_dst, dbuf, sizeof(dbuf));
				inet_ntop(AF_INET, &ip->ip_src, sbuf, sizeof(sbuf));
			}
			switch (log_in_vain) {
			case 1:
				if (thflags & TH_SYN) {
					log(LOG_INFO,
					    "Connection attempt to TCP %s:%d from %s:%d\n",
					    dbuf, ntohs(th->th_dport),
					    sbuf,
					    ntohs(th->th_sport));
				}
				break;
			case 2:
				log(LOG_INFO,
				    "Connection attempt to TCP %s:%d from %s:%d flags:0x%x\n",
				    dbuf, ntohs(th->th_dport), sbuf,
				    ntohs(th->th_sport), thflags);
				break;
			case 3:
			case 4:
				if ((thflags & TH_SYN) && !(thflags & TH_ACK) &&
				    !(m->m_flags & (M_BCAST | M_MCAST)) &&
				    ((isipv6 && !in6_are_addr_equal_scoped(&ip6->ip6_dst, &ip6->ip6_src, ip6_input_getdstifscope(m), ip6_input_getsrcifscope(m))) ||
				    (!isipv6 && ip->ip_dst.s_addr != ip->ip_src.s_addr))) {
					log_in_vain_log((LOG_INFO,
					    "Stealth Mode connection attempt to TCP %s:%d from %s:%d\n",
					    dbuf, ntohs(th->th_dport),
					    sbuf,
					    ntohs(th->th_sport)));
				}
				break;
			default:
				break;
			}
		}
		if (blackhole) {
			if (m->m_pkthdr.rcvif && m->m_pkthdr.rcvif->if_type != IFT_LOOP) {
				switch (blackhole) {
				case 1:
					if (thflags & TH_SYN) {
						TCP_LOG_DROP_PKT(TCP_LOG_HDR, th, ifp, "blackhole 1 syn for closed port");
						goto dropnosock;
					}
					break;
				case 2:
					TCP_LOG_DROP_PKT(TCP_LOG_HDR, th, ifp, "blackhole 2 closed port");
					goto dropnosock;
				default:
					TCP_LOG_DROP_PKT(TCP_LOG_HDR, th, ifp, "blackhole closed port");
					goto dropnosock;
				}
			}
		}
		IF_TCP_STATINC(ifp, noconnnolist);
		TCP_LOG_DROP_PKT(TCP_LOG_HDR, th, ifp, "closed port");
		goto dropwithresetnosock;
	}
	so = inp->inp_socket;
	if (so == NULL) {
		/* This case shouldn't happen  as the socket shouldn't be null
		 * if inp_state isn't set to INPCB_STATE_DEAD
		 * But just in case, we pretend we didn't find the socket if we hit this case
		 * as this isn't cause for a panic (the socket might be leaked however)...
		 */
		inp = NULL;
#if TEMPDEBUG
		printf("tcp_input: no more socket for inp=%x. This shouldn't happen\n", inp);
#endif
		TCP_LOG_DROP_PKT(TCP_LOG_HDR, th, ifp, "inp_socket NULL");
		goto dropnosock;
	}

	socket_lock(so, 1);
	if (in_pcb_checkstate(inp, WNT_RELEASE, 1) == WNT_STOPUSING) {
		socket_unlock(so, 1);
		inp = NULL;     // pretend we didn't find it
		TCP_LOG_DROP_PKT(TCP_LOG_HDR, th, ifp, "inp state WNT_STOPUSING");
		goto dropnosock;
	}

	if (!isipv6 && inp->inp_faddr.s_addr != INADDR_ANY) {
		if (inp->inp_faddr.s_addr != ip->ip_src.s_addr ||
		    inp->inp_laddr.s_addr != ip->ip_dst.s_addr ||
		    inp->inp_fport != th->th_sport ||
		    inp->inp_lport != th->th_dport) {
			os_log_error(OS_LOG_DEFAULT, "%s 5-tuple does not match: %u:%u %u:%u\n",
			    __func__,
			    ntohs(inp->inp_fport), ntohs(th->th_sport),
			    ntohs(inp->inp_lport), ntohs(th->th_dport));
			if (findpcb_iterated) {
				goto drop;
			}
			findpcb_iterated = true;
			socket_unlock(so, 1);
			inp = NULL;
			goto findpcb;
		}
	} else if (isipv6 && !IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr)) {
		if (!in6_are_addr_equal_scoped(&inp->in6p_faddr, &ip6->ip6_src, inp->inp_fifscope, ip6_input_getsrcifscope(m)) ||
		    !in6_are_addr_equal_scoped(&inp->in6p_laddr, &ip6->ip6_dst, inp->inp_lifscope, ip6_input_getdstifscope(m)) ||
		    inp->inp_fport != th->th_sport ||
		    inp->inp_lport != th->th_dport) {
			os_log_error(OS_LOG_DEFAULT, "%s 5-tuple does not match: %u:%u %u:%u\n",
			    __func__,
			    ntohs(inp->inp_fport), ntohs(th->th_sport),
			    ntohs(inp->inp_lport), ntohs(th->th_dport));
			if (findpcb_iterated) {
				goto drop;
			}
			findpcb_iterated = true;
			socket_unlock(so, 1);
			inp = NULL;
			goto findpcb;
		}
	}

	tp = intotcpcb(inp);
	if (tp == NULL) {
		IF_TCP_STATINC(ifp, noconnlist);
		TCP_LOG_DROP_PKT(TCP_LOG_HDR, th, ifp, "tp is NULL");
		goto dropwithreset;
	}

	/* Now that we found the tcpcb, we can adjust the TCP timestamp */
	if (to.to_flags & TOF_TS) {
		to.to_tsecr -= tp->t_ts_offset;
	}

	if (tp->t_state == TCPS_CLOSED) {
		TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "tp state TCPS_CLOSED");
		goto drop;
	}

#if NECP
	if (so->so_state & SS_ISCONNECTED) {
		// Connected TCP sockets have a fully-bound local and remote,
		// so the policy check doesn't need to override addresses
		if (!necp_socket_is_allowed_to_send_recv(inp, ifp, pf_tag, NULL, NULL, NULL, NULL)) {
			TCP_LOG_DROP_NECP(TCP_LOG_HDR, th, intotcpcb(inp), false);
			IF_TCP_STATINC(ifp, badformat);
			goto drop;
		}
	} else {
		/*
		 * If the proc_uuid_policy table has been updated since the last use
		 * of the listening socket (i.e., the proc_uuid_policy_table_gencount
		 * has been updated), the flags in the socket may be out of date.
		 * If INP2_WANT_APP_POLICY is stale, inbound packets may
		 * be dropped by NECP if the socket should now match a per-app
		 * exception policy.
		 * In order to avoid this refresh the proc_uuid_policy state to
		 * potentially recalculate the socket's flags before checking
		 * with NECP.
		 */
		(void) inp_update_policy(inp);

		if (isipv6) {
			if (!necp_socket_is_allowed_to_send_recv_v6(inp,
			    th->th_dport, th->th_sport, &ip6->ip6_dst,
			    &ip6->ip6_src, ifp, pf_tag, NULL, NULL, NULL, NULL)) {
				TCP_LOG_DROP_NECP(TCP_LOG_HDR, th, intotcpcb(inp), false);
				IF_TCP_STATINC(ifp, badformat);
				goto drop;
			}
		} else {
			if (!necp_socket_is_allowed_to_send_recv_v4(inp,
			    th->th_dport, th->th_sport, &ip->ip_dst, &ip->ip_src,
			    ifp, pf_tag, NULL, NULL, NULL, NULL)) {
				TCP_LOG_DROP_NECP(TCP_LOG_HDR, th, intotcpcb(inp), false);
				IF_TCP_STATINC(ifp, badformat);
				goto drop;
			}
		}
	}
#endif /* NECP */

	prev_t_state = tp->t_state;

	/* If none of the FIN|SYN|RST|ACK flag is set, drop */
	if ((thflags & TH_ACCEPT) == 0) {
		TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "rfc5961 TH_ACCEPT == 0");
		goto drop;
	}

	/* Initialize highest sacked seq to avoid using 0 as initial value */
	highest_sacked_seq = th->th_ack;

	/* Unscale the window into a 32-bit value. */
	if ((thflags & TH_SYN) == 0) {
		tiwin = th->th_win << tp->snd_scale;
	} else {
		tiwin = th->th_win;
	}

	/* Avoid processing packets while closing a listen socket */
	if (tp->t_state == TCPS_LISTEN &&
	    (so->so_options & SO_ACCEPTCONN) == 0) {
		TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "closing a listening socket");
		goto drop;
	}

	if ((m->m_flags & M_PKTHDR) && (m->m_pkthdr.pkt_flags & PKTF_WAKE_PKT)) {
		soevent(so, SO_FILT_HINT_LOCKED | SO_FILT_HINT_WAKE_PKT);
	}

	if (so->so_options & (SO_DEBUG | SO_ACCEPTCONN)) {
		if (so->so_options & SO_ACCEPTCONN) {
			struct tcpcb *tp0 = tp;
			struct socket *so2;
			struct socket *oso;
			struct sockaddr_storage from;
			struct sockaddr_storage to2;
			struct inpcb *oinp = sotoinpcb(so);
			struct ifnet *head_ifscope;
			bool head_nocell, head_recvanyif,
			    head_noexpensive, head_awdl_unrestricted,
			    head_intcoproc_allowed, head_external_port,
			    head_noconstrained, head_management_allowed,
			    head_ultra_constrained_allowed;

			/* Get listener's bound-to-interface, if any */
			head_ifscope = (inp->inp_flags & INP_BOUND_IF) ?
			    inp->inp_boundifp : NULL;
			/* Get listener's no-cellular information, if any */
			head_nocell = INP_NO_CELLULAR(inp);
			/* Get listener's recv-any-interface, if any */
			head_recvanyif = (inp->inp_flags & INP_RECV_ANYIF);
			/* Get listener's no-expensive information, if any */
			head_noexpensive = INP_NO_EXPENSIVE(inp);
			head_noconstrained = INP_NO_CONSTRAINED(inp);
			head_awdl_unrestricted = INP_AWDL_UNRESTRICTED(inp);
			head_intcoproc_allowed = INP_INTCOPROC_ALLOWED(inp);
			head_external_port = (inp->inp_flags2 & INP2_EXTERNAL_PORT);
			head_management_allowed = INP_MANAGEMENT_ALLOWED(inp);
			head_ultra_constrained_allowed = INP_ULTRA_CONSTRAINED_ALLOWED(inp);

			/*
			 * If the state is LISTEN then ignore segment if it contains an RST.
			 * If the segment contains an ACK then it is bad and send a RST.
			 * If it does not contain a SYN then it is not interesting; drop it.
			 * If it is from this socket, drop it, it must be forged.
			 */
			if ((thflags & (TH_RST | TH_ACK | TH_SYN)) != TH_SYN) {
				IF_TCP_STATINC(ifp, listbadsyn);

				if (thflags & TH_RST) {
					TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false,
					    thflags & TH_SYN ? "ignore SYN with RST" : "ignore RST");
					goto drop;
				}
				if (thflags & TH_ACK) {
					TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false,
					    thflags & TH_SYN ? "bad SYN with ACK" : "bad ACK");
					tp = NULL;
					tcpstat.tcps_badsyn++;
					goto dropwithreset;
				}

				/* We come here if there is no SYN set */
				tcpstat.tcps_badsyn++;
				TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "bad SYN");
				goto drop;
			}
			KERNEL_DEBUG(DBG_FNC_TCP_NEWCONN | DBG_FUNC_START, 0, 0, 0, 0, 0);
			if (th->th_dport == th->th_sport) {
				if (isipv6) {
					if (in6_are_addr_equal_scoped(&ip6->ip6_dst, &ip6->ip6_src, ip6_input_getdstifscope(m), ip6_input_getsrcifscope(m))) {
						TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "bad tuple same port");
						goto drop;
					}
				} else if (ip->ip_dst.s_addr == ip->ip_src.s_addr) {
					TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "bad tuple same IPv4 address");
					goto drop;
				}
			}
			/*
			 * RFC1122 4.2.3.10, p. 104: discard bcast/mcast SYN
			 * in_broadcast() should never return true on a received
			 * packet with M_BCAST not set.
			 *
			 * Packets with a multicast source address should also
			 * be discarded.
			 */
			if (m->m_flags & (M_BCAST | M_MCAST)) {
				TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "mbuf M_BCAST | M_MCAST");
				goto drop;
			}
			if (isipv6) {
				if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
				    IN6_IS_ADDR_MULTICAST(&ip6->ip6_src)) {
					TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "IN6_IS_ADDR_MULTICAST");
					goto drop;
				}
			} else if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) ||
			    IN_MULTICAST(ntohl(ip->ip_src.s_addr)) ||
			    ip->ip_src.s_addr == htonl(INADDR_BROADCAST) ||
			    in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif)) {
				TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "multicast or broadcast address");
				goto drop;
			}


			/*
			 * If deprecated address is forbidden,
			 * we do not accept SYN to deprecated interface
			 * address to prevent any new inbound connection from
			 * getting established.
			 * When we do not accept SYN, we send a TCP RST,
			 * with deprecated source address (instead of dropping
			 * it).  We compromise it as it is much better for peer
			 * to send a RST, and RST will be the final packet
			 * for the exchange.
			 *
			 * If we do not forbid deprecated addresses, we accept
			 * the SYN packet.  RFC 4862 forbids dropping SYN in
			 * this case.
			 */
			if (isipv6 && !ip6_use_deprecated) {
				uint32_t ia6_flags;

				if (ip6_getdstifaddr_info(m, NULL,
				    &ia6_flags) == 0) {
					if (ia6_flags & IN6_IFF_DEPRECATED) {
						tp = NULL;
						IF_TCP_STATINC(ifp, deprecate6);
						TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "deprecated IPv6 address");
						goto dropwithreset;
					}
				}
			}
			if (so->so_filt || check_cfil) {
				if (isipv6) {
					struct sockaddr_in6     *sin6 = SIN6(&from);

					sin6->sin6_len = sizeof(*sin6);
					sin6->sin6_family = AF_INET6;
					sin6->sin6_port = th->th_sport;
					sin6->sin6_flowinfo = 0;
					sin6->sin6_addr = ip6->ip6_src;
					sin6->sin6_scope_id = 0;

					sin6 = SIN6(&to2);

					sin6->sin6_len = sizeof(struct sockaddr_in6);
					sin6->sin6_family = AF_INET6;
					sin6->sin6_port = th->th_dport;
					sin6->sin6_flowinfo = 0;
					sin6->sin6_addr = ip6->ip6_dst;
					sin6->sin6_scope_id = 0;
				} else {
					struct sockaddr_in *sin = SIN(&from);

					sin->sin_len = sizeof(*sin);
					sin->sin_family = AF_INET;
					sin->sin_port = th->th_sport;
					sin->sin_addr = ip->ip_src;

					sin = SIN(&to2);

					sin->sin_len = sizeof(struct sockaddr_in);
					sin->sin_family = AF_INET;
					sin->sin_port = th->th_dport;
					sin->sin_addr = ip->ip_dst;
				}
			}

			if (so->so_filt) {
				so2 = sonewconn(so, 0, SA(&from));
			} else {
				so2 = sonewconn(so, 0, NULL);
			}
			if (so2 == 0) {
				tcpstat.tcps_listendrop++;
				if (tcp_dropdropablreq(so)) {
					if (so->so_filt) {
						so2 = sonewconn(so, 0, SA(&from));
					} else {
						so2 = sonewconn(so, 0, NULL);
					}
				}
				if (!so2) {
					TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, " listen drop");
					goto drop;
				}
			}

			/* Point "inp" and "tp" in tandem to new socket */
			inp = (struct inpcb *)so2->so_pcb;
			tp = intotcpcb(inp);

			oso = so;
			socket_unlock(so, 0); /* Unlock but keep a reference on listener for now */

			so = so2;
			socket_lock(so, 1);
			/*
			 * Mark socket as temporary until we're
			 * committed to keeping it.  The code at
			 * ``drop'' and ``dropwithreset'' check the
			 * flag dropsocket to see if the temporary
			 * socket created here should be discarded.
			 * We mark the socket as discardable until
			 * we're committed to it below in TCPS_LISTEN.
			 * There are some error conditions in which we
			 * have to drop the temporary socket.
			 */
			dropsocket++;
			/*
			 * Inherit INP_BOUND_IF from listener; testing if
			 * head_ifscope is non-NULL is sufficient, since it
			 * can only be set to a non-zero value earlier if
			 * the listener has such a flag set.
			 */
			if (head_ifscope != NULL) {
				inp->inp_flags |= INP_BOUND_IF;
				inp->inp_boundifp = head_ifscope;
			} else {
				inp->inp_flags &= ~INP_BOUND_IF;
			}
			/*
			 * Inherit restrictions from listener.
			 */
			if (head_nocell) {
				inp_set_nocellular(inp);
			}
			if (head_noexpensive) {
				inp_set_noexpensive(inp);
			}
			if (head_noconstrained) {
				inp_set_noconstrained(inp);
			}
			if (head_awdl_unrestricted) {
				inp_set_awdl_unrestricted(inp);
			}
			if (head_intcoproc_allowed) {
				inp_set_intcoproc_allowed(inp);
			}
			if (head_management_allowed) {
				inp_set_management_allowed(inp);
			}
			if (head_ultra_constrained_allowed) {
				inp_set_ultra_constrained_allowed(inp);
			}
			/*
			 * Inherit {IN,IN6}_RECV_ANYIF from listener.
			 */
			if (head_recvanyif) {
				inp->inp_flags |= INP_RECV_ANYIF;
			} else {
				inp->inp_flags &= ~INP_RECV_ANYIF;
			}

			if (head_external_port) {
				inp->inp_flags2 |= INP2_EXTERNAL_PORT;
			}
			if (isipv6) {
				inp->in6p_laddr = ip6->ip6_dst;
				inp->inp_lifscope = in6_addr2scopeid(ifp, &inp->in6p_laddr);
				in6_verify_ifscope(&ip6->ip6_dst, inp->inp_lifscope);
			} else {
				inp->inp_vflag &= ~INP_IPV6;
				inp->inp_vflag |= INP_IPV4;
				inp->inp_laddr = ip->ip_dst;
			}
			inp->inp_lport = th->th_dport;
			if (in_pcbinshash(inp, SA(&from), 0) != 0) {
				/*
				 * Undo the assignments above if we failed to
				 * put the PCB on the hash lists.
				 */
				if (isipv6) {
					inp->in6p_laddr = in6addr_any;
					inp->inp_lifscope = IFSCOPE_NONE;
				} else {
					inp->inp_laddr.s_addr = INADDR_ANY;
				}
#if SKYWALK
				netns_release(&inp->inp_netns_token);
#endif /* SKYWALK */
				inp->inp_lport = 0;
				socket_lock(oso, 0);    /* release ref on parent */
				socket_unlock(oso, 1);
				TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, " in_pcbinshash failed");
				goto drop;
			}
			socket_lock(oso, 0);
			if (isipv6) {
				/*
				 * Inherit socket options from the listening
				 * socket.
				 * Note that in6p_inputopts are not (even
				 * should not be) copied, since it stores
				 * previously received options and is used to
				 * detect if each new option is different than
				 * the previous one and hence should be passed
				 * to a user.
				 * If we copied in6p_inputopts, a user would
				 * not be able to receive options just after
				 * calling the accept system call.
				 */
				inp->inp_flags |=
				    oinp->inp_flags & INP_CONTROLOPTS;
				if (oinp->in6p_outputopts) {
					inp->in6p_outputopts =
					    ip6_copypktopts(oinp->in6p_outputopts,
					    Z_NOWAIT);
				}
			} else {
				inp->inp_options = ip_srcroute();
				inp->inp_ip_tos = oinp->inp_ip_tos;
			}
#if IPSEC
			/* copy old policy into new socket's */
			if (sotoinpcb(oso)->inp_sp) {
				int error = 0;
				/* Is it a security hole here to silently fail to copy the policy? */
				if (inp->inp_sp == NULL) {
					error = ipsec_init_policy(so, &inp->inp_sp);
				}
				if (error != 0 || ipsec_copy_policy(sotoinpcb(oso)->inp_sp, inp->inp_sp)) {
					printf("tcp_input: could not copy policy\n");
				}
			}
#endif
			/* inherit states from the listener */
			DTRACE_TCP4(state__change, void, NULL, struct inpcb *, inp,
			    struct tcpcb *, tp, int32_t, TCPS_LISTEN);
			TCP_LOG_STATE(tp, TCPS_LISTEN);
			tp->t_state = TCPS_LISTEN;
			tp->t_flags |= tp0->t_flags & (TF_NOPUSH | TF_NOOPT | TF_NODELAY);
			tp->t_flagsext |= (tp0->t_flagsext & (TF_RXTFINDROP | TF_NOTIMEWAIT | TF_FASTOPEN));
			tp->t_keepinit = tp0->t_keepinit;
			tp->t_keepcnt = tp0->t_keepcnt;
			tp->t_keepintvl = tp0->t_keepintvl;
			tp->t_adaptive_wtimo = tp0->t_adaptive_wtimo;
			tp->t_adaptive_rtimo = tp0->t_adaptive_rtimo;
			tp->t_inpcb->inp_ip_ttl = tp0->t_inpcb->inp_ip_ttl;
			if ((so->so_flags & SOF_NOTSENT_LOWAT) != 0) {
				tp->t_notsent_lowat = tp0->t_notsent_lowat;
			}
			tp->t_inpcb->inp_flags2 |=
			    tp0->t_inpcb->inp_flags2 & INP2_KEEPALIVE_OFFLOAD;

			/* now drop the reference on the listener */
			socket_unlock(oso, 1);

			tcp_set_max_rwinscale(tp, so);

#if CONTENT_FILTER
			if (check_cfil) {
				int error = cfil_sock_attach(so2, SA(&to2), SA(&from), CFS_CONNECTION_DIR_IN);
				if (error != 0) {
					TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, " cfil_sock_attach failed");
					goto drop;
				}
			}
#endif /* CONTENT_FILTER */

			KERNEL_DEBUG(DBG_FNC_TCP_NEWCONN | DBG_FUNC_END, 0, 0, 0, 0, 0);
		}
	}
	socket_lock_assert_owned(so);

	/*
	 * Packet accounting should not be done on listening socket
	 */
	if (th->th_flags & TH_SYN) {
		(void) os_add_overflow(1, tp->t_syn_rcvd, &tp->t_syn_rcvd);
	}
	if (th->th_flags & TH_FIN) {
		(void) os_add_overflow(1, tp->t_fin_rcvd, &tp->t_fin_rcvd);
	}
	if (th->th_flags & TH_RST) {
		(void) os_add_overflow(1, tp->t_rst_rcvd, &tp->t_rst_rcvd);
	}
	TCP_LOG_TH_FLAGS(TCP_LOG_HDR, th, tp, false, ifp);

	if (net_mpklog_enabled && (m->m_pkthdr.rcvif->if_xflags & IFXF_MPK_LOG)) {
		MPKL_TCP_INPUT(tcp_mpkl_log_object,
		    ntohs(tp->t_inpcb->inp_lport), ntohs(tp->t_inpcb->inp_fport),
		    th->th_seq, th->th_ack, tlen, thflags,
		    so->last_pid, so->so_log_seqn++);
	}

	if (tp->t_state == TCPS_ESTABLISHED && tlen > 0) {
		/*
		 * Evaluate the rate of arrival of packets to see if the
		 * receiver can reduce the ack traffic. The algorithm to
		 * stretch acks will be enabled if the connection meets
		 * certain criteria defined in tcp_stretch_ack_enable function.
		 */
		if ((tp->t_flagsext & TF_RCVUNACK_WAITSS) != 0) {
			TCP_INC_VAR(tp->rcv_waitforss, segment_count);
		}
		if (tcp_stretch_ack_enable(tp, thflags)) {
			tp->t_flags |= TF_STRETCHACK;
			tp->t_flagsext &= ~(TF_RCVUNACK_WAITSS);
			tp->rcv_waitforss = 0;
		} else {
			tp->t_flags &= ~(TF_STRETCHACK);
		}
		if (TSTMP_GT(tp->rcv_unackwin - (tcp_rcvunackwin >> 1), tcp_now)) {
			tp->rcv_by_unackhalfwin += (tlen + off);
			tp->rcv_by_unackwin += (tlen + off);
		} else {
			tp->rcv_unackwin = tcp_now + tcp_rcvunackwin;
			tp->rcv_by_unackwin = tp->rcv_by_unackhalfwin + tlen + off;
			tp->rcv_by_unackhalfwin = tlen + off;
		}
	}

	if (TCP_L4S_ENABLED(tp) && TCP_ACC_ECN_ON(tp)) {
		/* Reset the state used for AccECN processing */
		tp->t_aecn.accecn_processed = 0;
	}

	if (tp->t_state == TCPS_ESTABLISHED && BYTES_ACKED(th, tp) > 0) {
		if (CC_ALGO(tp)->set_bytes_acked != NULL) {
			CC_ALGO(tp)->set_bytes_acked(tp, BYTES_ACKED(th, tp));
		}
		if (tp->ecn_flags & TE_SENDIPECT) {
			/*
			 * Data sent with ECT has been acknowledged, calculate
			 * packets approx. by dividing by MSS. This is done to
			 * count MSS sized packets in case packets are aggregated
			 * by GRO/LRO.
			 */
			uint32_t bytes_acked = tcp_round_to(BYTES_ACKED(th, tp), tp->t_maxseg);
			tp->t_ecn_capable_packets_acked += max(1, (bytes_acked / tp->t_maxseg));
		}
	}

	/* Accurate ECN has different semantics for TH_CWR. */
	if (!TCP_ACC_ECN_ENABLED(tp)) {
		/*
		 * Clear TE_SENDECE if TH_CWR is set. This is harmless, so we don't
		 * bother doing extensive checks for state and whatnot.
		 */
		if (thflags & TH_CWR) {
			tp->ecn_flags &= ~TE_SENDECE;
			tp->t_ecn_recv_cwr++;
		}
	}

	/*
	 * Accurate ECN feedback for Data Receiver,
	 * Process IP ECN bits and update r.cep for CE marked pure ACKs
	 * or valid data packets
	 */
	uint8_t ace = tcp_get_ace(th);
	if (TCP_ACC_ECN_ON(tp) && tp->t_state == TCPS_ESTABLISHED) {
		/* Update receive side counters */
		if (tlen == 0 || (tlen > 0 &&
		    SEQ_GEQ(th->th_seq, tp->last_ack_sent) &&
		    SEQ_LT(th->th_seq, tp->last_ack_sent + tp->rcv_wnd))) {
			tcp_input_ip_ecn(tp, inp, (uint32_t)tlen, (uint32_t)segment_count, ip_ecn);
		}

		/* Test for ACE bleaching, initial value of ace should be non-zero */
		if (th->th_seq == tp->iss + 1 && ace == 0) {
			tp->t_client_accecn_state = tcp_connection_client_accurate_ecn_ace_bleaching_detected;
		}
	} else {
		/*
		 * Explicit Congestion Notification - Flag that we need to send ECE if
		 *	+ The IP Congestion experienced flag was set.
		 *	+ Socket is in established state
		 *	+ We negotiated ECN in the TCP setup
		 *	+ This isn't a pure ack (tlen > 0)
		 *	+ The data is in the valid window
		 *
		 *	TE_SENDECE will be cleared when we receive a packet with TH_CWR set.
		 */
		if (ip_ecn == IPTOS_ECN_CE && tp->t_state == TCPS_ESTABLISHED &&
		    TCP_ECN_ENABLED(tp) && tlen > 0 &&
		    SEQ_GEQ(th->th_seq, tp->last_ack_sent) &&
		    SEQ_LT(th->th_seq, tp->last_ack_sent + tp->rcv_wnd)) {
			tp->t_ecn_recv_ce++;
			tcpstat.tcps_ecn_recv_ce++;
			INP_INC_IFNET_STAT(inp, ecn_recv_ce);
			/* Mark this connection as it received CE from network */
			tp->ecn_flags |= TE_RECV_ECN_CE;
			tp->ecn_flags |= TE_SENDECE;
		}
	}

	/*
	 * If we received an explicit notification of congestion in
	 * ip tos ecn bits or by the CWR bit in TCP header flags, reset
	 * the ack-stretching state. We need to handle ECN notification if
	 * an ECN setup SYN was sent even once.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	    (tp->ecn_flags & TE_SETUPSENT) &&
	    (ip_ecn == IPTOS_ECN_CE || (thflags & TH_CWR))) {
		tcp_reset_stretch_ack(tp);
		tp->t_forced_acks = TCP_FORCED_ACKS_COUNT;
		CLEAR_IAJ_STATE(tp);
	}

	if (ip_ecn == IPTOS_ECN_CE && tp->t_state == TCPS_ESTABLISHED &&
	    !TCP_ECN_ENABLED(tp) && !(tp->ecn_flags & TE_CEHEURI_SET)) {
		tcpstat.tcps_ecn_fallback_ce++;
		tcp_heuristic_ecn_aggressive(tp);
		tp->ecn_flags |= TE_CEHEURI_SET;
	}

	if (tp->t_state == TCPS_ESTABLISHED && TCP_ECN_ENABLED(tp) &&
	    ip_ecn == IPTOS_ECN_CE && !(tp->ecn_flags & TE_CEHEURI_SET)) {
		if (inp->inp_stat->rxpackets < ECN_MIN_CE_PROBES) {
			tp->t_ecn_recv_ce_pkt++;
		} else if (tp->t_ecn_recv_ce_pkt > ECN_MAX_CE_RATIO) {
			tcpstat.tcps_ecn_fallback_ce++;
			tcp_heuristic_ecn_aggressive(tp);
			tp->ecn_flags |= TE_CEHEURI_SET;
			INP_INC_IFNET_STAT(inp, ecn_fallback_ce);
		} else {
			/* We tracked the first ECN_MIN_CE_PROBES segments, we
			 * now know that the path is good.
			 */
			tp->ecn_flags |= TE_CEHEURI_SET;
		}
	}

	/* Update rcvtime as a new segment was received on the connection */
	tp->t_rcvtime = tcp_now;

	/*
	 * Segment received on connection.
	 * Reset idle time and keep-alive timer.
	 */
	if (TCPS_HAVEESTABLISHED(tp->t_state)) {
		tcp_keepalive_reset(tp);

		if (tp->t_mpsub) {
			mptcp_reset_keepalive(tp);
		}
	}

	/*
	 * Process options if not in LISTEN state,
	 * else do it below (after getting remote address).
	 */
	if (tp->t_state != TCPS_LISTEN && optp) {
		tcp_dooptions(tp, optp, optlen, th, &to);
	}
#if MPTCP
	if (tp->t_state != TCPS_LISTEN && (so->so_flags & SOF_MP_SUBFLOW)) {
		mptcp_insert_rmap(tp, m, th);
	}
#endif /* MPTCP */
	if (tp->t_state == TCPS_SYN_SENT && (thflags & TH_SYN)) {
		if (!(thflags & TH_ACK) ||
		    (SEQ_GT(th->th_ack, tp->iss) &&
		    SEQ_LEQ(th->th_ack, tp->snd_max))) {
			tcp_finalize_options(tp, &to, ifscope);
		}
	}

#if TRAFFIC_MGT
	/*
	 * Compute inter-packet arrival jitter. According to RFC 3550,
	 * inter-packet arrival jitter is defined as the difference in
	 * packet spacing at the receiver compared to the sender for a
	 * pair of packets. When two packets of maximum segment size come
	 * one after the other with consecutive sequence numbers, we
	 * consider them as packets sent together at the sender and use
	 * them as a pair to compute inter-packet arrival jitter. This
	 * metric indicates the delay induced by the network components due
	 * to queuing in edge/access routers.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	    (thflags & (TH_SYN | TH_FIN | TH_RST | TH_URG | TH_ACK | TH_ECE | TH_PUSH)) == TH_ACK &&
	    ((tp->t_flags & TF_NEEDFIN) == 0) &&
	    ((to.to_flags & TOF_TS) == 0 ||
	    TSTMP_GEQ(to.to_tsval, tp->ts_recent)) &&
	    th->th_seq == tp->rcv_nxt && LIST_EMPTY(&tp->t_segq)) {
		int seg_size = tlen;
		if (tp->iaj_pktcnt <= IAJ_IGNORE_PKTCNT) {
			TCP_INC_VAR(tp->iaj_pktcnt, segment_count);
		}

		if (tp->iaj_size == 0 || seg_size > tp->iaj_size ||
		    (seg_size == tp->iaj_size && tp->iaj_rcv_ts == 0)) {
			/*
			 * State related to inter-arrival jitter is
			 * uninitialized or we are trying to find a good
			 * first packet to start computing the metric
			 */
			update_iaj_state(tp, seg_size, 0);
		} else {
			if (seg_size == tp->iaj_size) {
				/*
				 * Compute inter-arrival jitter taking
				 * this packet as the second packet
				 */
				compute_iaj(tp);
			}
			if (seg_size < tp->iaj_size) {
				/*
				 * There is a smaller packet in the stream.
				 * Some times the maximum size supported
				 * on a path can change if there is a new
				 * link with smaller MTU. The receiver will
				 * not know about this change. If there
				 * are too many packets smaller than
				 * iaj_size, we try to learn the iaj_size
				 * again.
				 */
				TCP_INC_VAR(tp->iaj_small_pkt, segment_count);
				if (tp->iaj_small_pkt > RESET_IAJ_SIZE_THRESH) {
					update_iaj_state(tp, seg_size, 1);
				} else {
					CLEAR_IAJ_STATE(tp);
				}
			} else {
				update_iaj_state(tp, seg_size, 0);
			}
		}
	} else {
		CLEAR_IAJ_STATE(tp);
	}
#endif /* TRAFFIC_MGT */

	/*
	 * Header prediction: check for the two common cases
	 * of a uni-directional data xfer.  If the packet has
	 * no control flags, is in-sequence, the window didn't
	 * change and we're not retransmitting, it's a
	 * candidate.  If the length is zero and the ack moved
	 * forward, we're the sender side of the xfer.  Just
	 * free the data acked & wake any higher level process
	 * that was blocked waiting for space.  If the length
	 * is non-zero and the ack didn't move, we're the
	 * receiver side.  If we're getting packets in-order
	 * (the reassembly queue is empty), add the data to
	 * the socket buffer and note that we need a delayed ack.
	 * Make sure that the hidden state-flags are also off.
	 * Since we check for TCPS_ESTABLISHED above, it can only
	 * be TH_NEEDSYN.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	    !(so->so_state & SS_CANTRCVMORE) &&
	    (thflags & TH_FLAGS) == TH_ACK &&
	    ((tp->t_flags & TF_NEEDFIN) == 0) &&
	    ((to.to_flags & TOF_TS) == 0 ||
	    TSTMP_GEQ(to.to_tsval, tp->ts_recent)) &&
	    th->th_seq == tp->rcv_nxt &&
	    tiwin && tiwin == tp->snd_wnd &&
	    tp->snd_nxt == tp->snd_max) {
		/*
		 * If last ACK falls within this segment's sequence numbers,
		 * record the timestamp.
		 * NOTE that the test is modified according to the latest
		 * proposal of the tcplw@cray.com list (Braden 1993/04/26).
		 */
		if ((to.to_flags & TOF_TS) != 0 &&
		    SEQ_LEQ(th->th_seq, tp->last_ack_sent)) {
			tp->ts_recent_age = tcp_now;
			tp->ts_recent = to.to_tsval;
		}

		/*
		 * We increment t_unacksegs_ce for both data segments
		 * and pure ACKs for Accurate ECN
		 */
		if (TCP_ACC_ECN_ON(tp) && ip_ecn == IPTOS_ECN_CE) {
			TCP_INC_VAR(tp->t_unacksegs_ce, segment_count);
		}

		if (tlen == 0) {
			if (SEQ_GT(th->th_ack, tp->snd_una) &&
			    SEQ_LEQ(th->th_ack, tp->snd_max) &&
			    tp->snd_cwnd >= tp->snd_ssthresh &&
			    (!IN_FASTRECOVERY(tp) &&
			    ((!(SACK_ENABLED(tp)) &&
			    tp->t_dupacks < tp->t_rexmtthresh) ||
			    (SACK_ENABLED(tp) && to.to_nsacks == 0 &&
			    TAILQ_EMPTY(&tp->snd_holes))))) {
				/*
				 * this is a pure ack for outstanding data.
				 */
				++tcpstat.tcps_predack;

				tcp_bad_rexmt_check(tp, th, &to);

				/* Recalculate the RTT */
				tcp_compute_rtt(tp, &to, th);

				VERIFY(SEQ_GEQ(th->th_ack, tp->snd_una));
				acked = BYTES_ACKED(th, tp);
				tcpstat.tcps_rcvackpack++;
				tcpstat.tcps_rcvackbyte += acked;

				/* TE_SENDIPECT is only set when L4S sysctl is enabled */
				if (TCP_ACC_ECN_ON(tp) && (tp->ecn_flags & TE_SENDIPECT)) {
					if (!TCP_L4S_ENABLED(tp)) {
						os_log_error(OS_LOG_DEFAULT, "TE_SENDIPECT flag is set but TCP_L4S_ENABLED is not");
					}
					uint32_t pkts_acked = tcp_packets_this_ack(tp, acked);
					tp->total_ect_packets_acked += pkts_acked;

					bool newly_acked_time = false;
					if (acked == 0 && (to.to_flags & TOF_TS) != 0 && to.to_tsecr != 0 &&
					    TSTMP_GT(to.to_tsecr, tp->t_last_ack_tsecr)) {
						newly_acked_time = true;
					}
					if (acked > 0 || newly_acked_time) {
						tcp_process_accecn(tp, &to, th, pkts_acked, ace);
					}
				}

				/*
				 * Process sent segments used for RACK, called after RTT is computed
				 * RACK reordering window doesn't need to be updated until we process
				 * DSACK.
				 */
				if (TCP_RACK_ENABLED(tp)) {
					tcp_segs_doack(tp, th->th_ack, &to);
					if (SEQ_LT(tp->snd_fack, th->th_ack)) {
						/*
						 * We update snd_fack here for RACK only as it is updated
						 * and used differently for SACK. This should be done after
						 * ACK processing of segments which checks for reordering.
						 * Also, we don't compare with highest_sacked_seq here as this
						 * is the fast path with no SACK blocks.
						 */
						tp->snd_fack = th->th_ack;
					}
				}

				/*
				 * Handle an ack that is in sequence during
				 * congestion avoidance phase. The
				 * calculations in this function
				 * assume that snd_una is not updated yet.
				 */
				if (CC_ALGO(tp)->congestion_avd != NULL) {
					CC_ALGO(tp)->congestion_avd(tp, th);
				}
				tcp_ccdbg_trace(tp, th, TCP_CC_INSEQ_ACK_RCVD);
				sbdrop(&so->so_snd, acked);
				tcp_sbsnd_trim(&so->so_snd);

				if (SEQ_GT(tp->snd_una, tp->snd_recover) &&
				    SEQ_LEQ(th->th_ack, tp->snd_recover)) {
					tp->snd_recover = th->th_ack - 1;
				}

				tcp_update_snd_una(tp, th->th_ack);

				TCP_RESET_REXMT_STATE(tp);

				/*
				 * pull snd_wl2 up to prevent seq wrap relative
				 * to th_ack.
				 */
				tp->snd_wl2 = th->th_ack;

				if (tp->t_dupacks > 0) {
					tp->t_dupacks = 0;
					tp->t_rexmtthresh = tcprexmtthresh;
				}

				tp->sackhint.sack_bytes_acked = 0;

				/*
				 * If all outstanding data are acked, stop
				 * retransmit timer, otherwise restart timer
				 * using current (possibly backed-off) value.
				 * If process is waiting for space,
				 * wakeup/selwakeup/signal.  If data
				 * are ready to send, let tcp_output
				 * decide between more output or persist.
				 */
				if (tp->snd_una == tp->snd_max) {
					tp->t_timer[TCPT_REXMT] = 0;
					tp->t_timer[TCPT_PTO] = 0;
					tp->t_timer[TCPT_REORDER] = 0;
					tcp_rack_reset_segs_retransmitted(tp);
				} else if (tp->t_timer[TCPT_PERSIST] == 0) {
					tp->t_timer[TCPT_REXMT] = OFFSET_FROM_START(tp, tp->t_rxtcur);
				}
				if (!SLIST_EMPTY(&tp->t_rxt_segments) &&
				    !TCP_DSACK_SEQ_IN_WINDOW(tp,
				    tp->t_dsack_lastuna, tp->snd_una)) {
					tcp_rxtseg_clean(tp);
				}

				if ((tp->t_flagsext & TF_MEASURESNDBW) != 0 &&
				    tp->t_bwmeas != NULL) {
					tcp_bwmeas_check(tp);
				}

				write_wakeup = 1;
				if (!SLIST_EMPTY(&tp->t_notify_ack)) {
					tcp_notify_acknowledgement(tp, so);
				}

				if ((so->so_snd.sb_cc) || (tp->t_flags & TF_ACKNOW)) {
					(void) tcp_output(tp);
				}

				tcp_tfo_rcv_ack(tp, th);

				m_freem(m);

				tcp_check_timer_state(tp);

				tcp_handle_wakeup(so, read_wakeup, write_wakeup);

				socket_unlock(so, 1);
				KERNEL_DEBUG(DBG_FNC_TCP_INPUT | DBG_FUNC_END, 0, 0, 0, 0, 0);
				return;
			}
		} else if (th->th_ack == tp->snd_una && LIST_EMPTY(&tp->t_segq) &&
		    tlen <= tcp_sbspace(tp)) {
			/*
			 * this is a pure, in-sequence data packet
			 * with nothing on the reassembly queue and
			 * we have enough buffer space to take it.
			 */

			/* Clean receiver SACK report if present */
			if (SACK_ENABLED(tp) && tp->rcv_numsacks) {
				tcp_clean_sackreport(tp);
			}
			++tcpstat.tcps_preddat;
			tp->rcv_nxt += tlen;
			/* Update highest received sequence and its timestamp */
			if (SEQ_LT(tp->rcv_high, tp->rcv_nxt)) {
				tp->rcv_high = tp->rcv_nxt;
				if (to.to_flags & TOF_TS) {
					tp->tsv_high = to.to_tsval;
				}
			}

			/*
			 * Pull snd_wl1 up to prevent seq wrap relative to
			 * th_seq.
			 */
			tp->snd_wl1 = th->th_seq;
			/*
			 * Pull rcv_up up to prevent seq wrap relative to
			 * rcv_nxt.
			 */
			tp->rcv_up = tp->rcv_nxt;
			TCP_INC_VAR(tcpstat.tcps_rcvpack, segment_count);
			tcpstat.tcps_rcvbyte += tlen;
			if (nstat_collect) {
				INP_ADD_STAT(inp, ifnet_count_type,
				    rxpackets, 1);
				INP_ADD_STAT(inp, ifnet_count_type, rxbytes,
				    tlen);
				inp_set_activity_bitmap(inp);
			}

			/* Calculate the RTT on the receiver */
			tcp_compute_rcv_rtt(tp, &to, th);

			tcp_sbrcv_grow(tp, &so->so_rcv, &to, tlen);
			if (TCP_USE_RLEDBAT(tp, so) && tcp_cc_rledbat.data_rcvd != NULL) {
				tcp_cc_rledbat.data_rcvd(tp, th, &to, tlen);
			}

			/*
			 * Add data to socket buffer.
			 */
			so_recv_data_stat(so, m, 0);
			m_adj(m, drop_hdrlen);  /* delayed header drop */

			if (isipv6) {
				memcpy(&saved_hdr, ip6, sizeof(struct ip6_hdr));
				ip6 = (struct ip6_hdr *)&saved_hdr[0];
			} else {
				memcpy(&saved_hdr, ip, ip->ip_hl << 2);
				ip = (struct ip *)&saved_hdr[0];
			}
			memcpy(&saved_tcphdr, th, sizeof(struct tcphdr));

			if (th->th_flags & TH_PUSH) {
				tp->t_flagsext |= TF_LAST_IS_PSH;
			} else {
				tp->t_flagsext &= ~TF_LAST_IS_PSH;
			}

			if (sbappendstream_rcvdemux(so, m)) {
				mptcp_handle_input(so);
				read_wakeup = 1;
			}
			th = &saved_tcphdr;

			if (isipv6) {
				KERNEL_DEBUG(DBG_LAYER_END, ((th->th_dport << 16) | th->th_sport),
				    (((ip6->ip6_src.s6_addr16[0]) << 16) | (ip6->ip6_dst.s6_addr16[0])),
				    th->th_seq, th->th_ack, th->th_win);
			} else {
				KERNEL_DEBUG(DBG_LAYER_END, ((th->th_dport << 16) | th->th_sport),
				    (((ip->ip_src.s_addr & 0xffff) << 16) | (ip->ip_dst.s_addr & 0xffff)),
				    th->th_seq, th->th_ack, th->th_win);
			}
			TCP_INC_VAR(tp->t_unacksegs, segment_count);
			if (DELAY_ACK(tp, th)) {
				if ((tp->t_flags & TF_DELACK) == 0) {
					tp->t_flags |= TF_DELACK;
					tp->t_timer[TCPT_DELACK] = OFFSET_FROM_START(tp, tcp_delack);
				}
			} else {
				tp->t_flags |= TF_ACKNOW;
				tcp_output(tp);
			}

			tcp_adaptive_rwtimo_check(tp, tlen);

			if (tlen > 0) {
				tcp_tfo_rcv_data(tp);
			}

			tcp_check_timer_state(tp);

			tcp_handle_wakeup(so, read_wakeup, write_wakeup);

			socket_unlock(so, 1);
			KERNEL_DEBUG(DBG_FNC_TCP_INPUT | DBG_FUNC_END, 0, 0, 0, 0, 0);
			return;
		}
	}

	/*
	 * Calculate amount of space in receive window,
	 * and then do TCP input processing.
	 * Receive window is amount of space in rcv queue,
	 * but not less than advertised window.
	 */
	socket_lock_assert_owned(so);
	win = tcp_sbspace(tp);
	if (win < 0) {
		win = 0;
	} else { /* clip rcv window to 4K for modems */
		if (tp->t_flags & TF_SLOWLINK && slowlink_wsize > 0) {
			win = min(win, slowlink_wsize);
		}
	}
	tp->rcv_wnd = imax(win, (int)(tp->rcv_adv - tp->rcv_nxt));
#if MPTCP
	/*
	 * Ensure that the subflow receive window isn't greater
	 * than the connection level receive window.
	 */
	if ((tp->t_mpflags & TMPF_MPTCP_TRUE) && (mp_tp = tptomptp(tp))) {
		socket_lock_assert_owned(mptetoso(mp_tp->mpt_mpte));
		int64_t recwin_conn = (int64_t)(mp_tp->mpt_rcvadv - mp_tp->mpt_rcvnxt);

		VERIFY(recwin_conn < INT32_MAX && recwin_conn > INT32_MIN);
		if (recwin_conn > 0 && tp->rcv_wnd > (uint32_t)recwin_conn) {
			tp->rcv_wnd = (uint32_t)recwin_conn;
			tcpstat.tcps_mp_reducedwin++;
		}
	}
#endif /* MPTCP */

	switch (tp->t_state) {
	/*
	 * Initialize tp->rcv_nxt, and tp->irs, select an initial
	 * tp->iss, and send a segment:
	 *		<SEQ=ISS><ACK=RCV_NXT><CTL=SYN,ACK>
	 * Also initialize tp->snd_nxt to tp->iss+1 and tp->snd_una to tp->iss.
	 * Fill in remote peer address fields if not previously specified.
	 * Enter SYN_RECEIVED state, and process any other fields of this
	 * segment in this state.
	 */
	case TCPS_LISTEN: {
		struct sockaddr_in *sin;
		struct sockaddr_in6 *sin6;

		socket_lock_assert_owned(so);

		/* Clear the logging flags inherited from the listening socket */
		inp->inp_log_flags = 0;
		inp->inp_flags2 |= INP2_LOGGED_SUMMARY;

		if (isipv6) {
			sin6 = kalloc_type(struct sockaddr_in6, Z_NOWAIT | Z_ZERO);
			if (sin6 == NULL) {
				TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "LISTEN kalloc_type failed");
				goto drop;
			}
			sin6->sin6_family = AF_INET6;
			sin6->sin6_len = sizeof(*sin6);
			sin6->sin6_addr = ip6->ip6_src;
			sin6->sin6_port = th->th_sport;
			if (!in6_embedded_scope && IN6_IS_SCOPE_EMBED(&ip6->ip6_src)) {
				sin6->sin6_scope_id = ip6_input_getsrcifscope(m);
			}
			laddr6 = inp->in6p_laddr;
			uint32_t lifscope = inp->inp_lifscope;
			if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)) {
				inp->in6p_laddr = ip6->ip6_dst;
				inp->inp_lifscope = in6_addr2scopeid(ifp, &inp->in6p_laddr);
				in6_verify_ifscope(&inp->in6p_laddr, inp->inp_lifscope);
			}
			if (in6_pcbconnect(inp, SA(sin6), kernel_proc)) {
				inp->in6p_laddr = laddr6;
				kfree_type(struct sockaddr_in6, sin6);
				inp->inp_lifscope = lifscope;
				in6_verify_ifscope(&inp->in6p_laddr, inp->inp_lifscope);
				TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, " LISTEN in6_pcbconnect failed");
				goto drop;
			}
			kfree_type(struct sockaddr_in6, sin6);
		} else {
			socket_lock_assert_owned(so);
			sin = kalloc_type(struct sockaddr_in, Z_NOWAIT);
			if (sin == NULL) {
				TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "LISTEN kalloc_type failed");
				goto drop;
			}
			sin->sin_family = AF_INET;
			sin->sin_len = sizeof(*sin);
			sin->sin_addr = ip->ip_src;
			sin->sin_port = th->th_sport;
			bzero((caddr_t)sin->sin_zero, sizeof(sin->sin_zero));
			laddr = inp->inp_laddr;
			if (inp->inp_laddr.s_addr == INADDR_ANY) {
				inp->inp_laddr = ip->ip_dst;
			}
			if (in_pcbconnect(inp, SA(sin), kernel_proc, IFSCOPE_NONE, NULL)) {
				inp->inp_laddr = laddr;
				kfree_type(struct sockaddr_in, sin);
				TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, " LISTEN in_pcbconnect failed");
				goto drop;
			}
			kfree_type(struct sockaddr_in, sin);
		}

		tcp_dooptions(tp, optp, optlen, th, &to);
		tcp_finalize_options(tp, &to, ifscope);

		if (TFO_ENABLED(tp) && tcp_tfo_syn(tp, &to)) {
			isconnected = TRUE;
		}

		if (iss) {
			tp->iss = iss;
		} else {
			tp->iss = tcp_new_isn(tp);
		}
		tp->irs = th->th_seq;
		tcp_sendseqinit(tp);
		tcp_rcvseqinit(tp);
		tp->snd_recover = tp->snd_una;
		/*
		 * Initialization of the tcpcb for transaction;
		 *   set SND.WND = SEG.WND,
		 *   initialize CCsend and CCrecv.
		 */
		tp->snd_wnd = tiwin;    /* initial send-window */
		tp->max_sndwnd = tp->snd_wnd;
		tp->t_flags |= TF_ACKNOW;
		tp->t_unacksegs = 0;
		tp->t_unacksegs_ce = 0;
		DTRACE_TCP4(state__change, void, NULL, struct inpcb *, inp,
		    struct tcpcb *, tp, int32_t, TCPS_SYN_RECEIVED);
		TCP_LOG_STATE(tp, TCPS_SYN_RECEIVED);
		tp->t_state = TCPS_SYN_RECEIVED;
		tp->t_timer[TCPT_KEEP] = OFFSET_FROM_START(tp,
		    TCP_CONN_KEEPINIT(tp));
		tp->t_connect_time = tcp_now;
		dropsocket = 0;         /* committed to socket */

		if (inp->inp_flowhash == 0) {
			inp_calc_flowhash(inp);
			ASSERT(inp->inp_flowhash != 0);
		}
		/* update flowinfo - RFC 6437 */
		if (inp->inp_flow == 0 &&
		    inp->in6p_flags & IN6P_AUTOFLOWLABEL) {
			inp->inp_flow &= ~IPV6_FLOWLABEL_MASK;
			inp->inp_flow |=
			    (htonl(ip6_randomflowlabel()) & IPV6_FLOWLABEL_MASK);
		}

		/* reset the incomp processing flag */
		so->so_flags &= ~(SOF_INCOMP_INPROGRESS);
		tcpstat.tcps_accepts++;

		int ace_flags = ((th->th_x2 << 8) | thflags) & TH_ACE;
		tcp_input_process_accecn_syn(tp, ace_flags, ip_ecn);

		/*
		 * The address and connection state are finalized
		 */
		TCP_LOG_CONNECT(tp, false, 0);

		tcp_add_fsw_flow(tp, ifp);

		goto trimthenstep6;
	}

	/*
	 * If the state is SYN_RECEIVED and the seg contains an ACK,
	 * but not for our SYN/ACK, send a RST.
	 */
	case TCPS_SYN_RECEIVED:
		if ((thflags & TH_ACK) &&
		    (SEQ_LEQ(th->th_ack, tp->snd_una) ||
		    SEQ_GT(th->th_ack, tp->snd_max))) {
			IF_TCP_STATINC(ifp, ooopacket);
			TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "SYN_RECEIVED bad ACK");
			goto dropwithreset;
		}

		/*
		 * In SYN_RECEIVED state, if we recv some SYNS with
		 * window scale and others without, window scaling should
		 * be disabled. Otherwise the window advertised will be
		 * lower if we assume scaling and the other end does not.
		 */
		if ((thflags & TH_SYN) &&
		    (tp->irs == th->th_seq) &&
		    !(to.to_flags & TOF_SCALE)) {
			tp->t_flags &= ~TF_RCVD_SCALE;
		}
		break;

	/*
	 * If the state is SYN_SENT:
	 *	if seg contains an ACK, but not for our SYN, drop the input.
	 *	if seg contains a RST, then drop the connection.
	 *	if seg does not contain SYN, then drop it.
	 * Otherwise this is an acceptable SYN segment
	 *	initialize tp->rcv_nxt and tp->irs
	 *	if seg contains ack then advance tp->snd_una
	 *	if SYN has been acked change to ESTABLISHED else SYN_RCVD state
	 *	arrange for segment to be acked (eventually)
	 *	continue processing rest of data/controls, beginning with URG
	 */
	case TCPS_SYN_SENT:
		if ((thflags & TH_ACK) &&
		    (SEQ_LEQ(th->th_ack, tp->iss) ||
		    SEQ_GT(th->th_ack, tp->snd_max))) {
			IF_TCP_STATINC(ifp, ooopacket);
			TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "SYN_SENT bad ACK");
			goto dropwithreset;
		}
		if (thflags & TH_RST) {
			if ((thflags & TH_ACK) != 0) {
				if (TFO_ENABLED(tp) &&
				    !(tp->t_flagsext & TF_FASTOPEN_FORCE_ENABLE)) {
					tcp_heuristic_tfo_rst(tp);
				}
				if ((tp->ecn_flags & (TE_SETUPSENT | TE_RCVD_SYN_RST)) == TE_SETUPSENT ||
				    (tp->ecn_flags & (TE_ACE_SETUPSENT | TE_RCVD_SYN_RST)) == TE_ACE_SETUPSENT) {
					/*
					 * On local connections, send
					 * non-ECN syn one time before
					 * dropping the connection
					 */
					if (tp->t_flags & TF_LOCAL) {
						tp->ecn_flags |= TE_RCVD_SYN_RST;
						drop_reason = DROP_REASON_TCP_RST;
						goto drop;
					} else {
						tcp_heuristic_ecn_synrst(tp);
					}
				}
				soevent(so,
				    (SO_FILT_HINT_LOCKED |
				    SO_FILT_HINT_CONNRESET));
				tp = tcp_drop(tp, ECONNREFUSED);
			}
			TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "SYN_SENT got RST");
			drop_reason = DROP_REASON_TCP_RST;
			goto drop;
		}
		if ((thflags & TH_SYN) == 0) {
			TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "SYN_SENT no SYN");
			goto drop;
		}
		tp->snd_wnd = th->th_win;       /* initial send window */
		tp->max_sndwnd = tp->snd_wnd;

		tp->irs = th->th_seq;
		tcp_rcvseqinit(tp);
		if (thflags & TH_ACK) {
			/* Client processes SYN-ACK */
			tcpstat.tcps_connects++;

			const uint32_t ace_flags = ((th->th_x2 << 8) | thflags) & TH_ACE;

			if ((thflags & (TH_ECE | TH_CWR)) == (TH_ECE)) {
				/* Receiving Any|0|1 is classic ECN-setup SYN-ACK */
				tp->ecn_flags |= TE_SETUPRECEIVED;
				if (TCP_ECN_ENABLED(tp)) {
					tcp_heuristic_ecn_success(tp);
					tcpstat.tcps_ecn_client_success++;
				}

				if (tp->ecn_flags & TE_ACE_SETUPSENT) {
					/*
					 * Sent AccECN SYN but received classic ECN SYN-ACK
					 * Set classic ECN related flags
					 */
					tp->ecn_flags |= (TE_SETUPSENT | TE_SENDIPECT);
					tp->ecn_flags &= ~TE_ACE_SETUPSENT;
					if (tp->t_client_accecn_state == tcp_connection_client_accurate_ecn_feature_enabled) {
						tp->t_client_accecn_state = tcp_connection_client_classic_ecn_available;
					}
				}
			} else if (TCP_ACC_ECN_ENABLED(tp) && ace_flags != 0 &&
			    ace_flags != TH_ACE) {
				/* Initialize sender side packet & byte counters */
				tp->t_aecn.t_snd_ce_packets = 5;
				tp->t_aecn.t_snd_ect1_bytes = tp->t_aecn.t_snd_ect0_bytes = 1;
				tp->t_aecn.t_snd_ce_bytes = 0;
				tp->ecn_flags |= TE_ACE_FINAL_ACK_3WHS;
				/*
				 * Client received AccECN SYN-ACK that reflects the state (ECN)
				 * in which SYN packet was delivered. This helps to detect if
				 * there was mangling of the SYN packet on the path. Currently, we
				 * only send Not-ECT on SYN packets. So, we should set Not-ECT in
				 * all packets if we receive any encoding other than 0|TH_CWR|0.
				 * If 0|0|0 and 1|1|1 were received, fail Accurate ECN negotiation
				 * by not setting TE_ACE_SETUPRECEIVED.
				 */
				uint32_t ecn_flags = TE_ACE_SETUPRECEIVED;
				if (TCP_L4S_ENABLED(tp)) {
					ecn_flags |= TE_SENDIPECT;
				}
				switch (ace_flags) {
				case (0 | TH_CWR | 0):
					/* Non-ECT SYN was delivered */
					tp->ecn_flags |= ecn_flags;
					tcpstat.tcps_ecn_ace_syn_not_ect++;
					tp->t_client_accecn_state = tcp_connection_client_accurate_ecn_negotiation_success;
					break;
				case (0 | TH_CWR | TH_ECE):
					/* ECT1 SYN was delivered */
					tp->ecn_flags |= ecn_flags;
					/* Mangling detected, set Non-ECT on outgoing packets */
					tp->ecn_flags &= ~TE_SENDIPECT;
					tcpstat.tcps_ecn_ace_syn_ect1++;
					tp->t_client_accecn_state = tcp_connection_client_accurate_ecn_negotiation_success_ect_mangling_detected;
					break;
				case (TH_AE | 0 | 0):
					/* ECT0 SYN was delivered */
					tp->ecn_flags |= ecn_flags;
					/* Mangling detected, set Non-ECT on outgoing packets */
					tp->ecn_flags &= ~TE_SENDIPECT;
					tcpstat.tcps_ecn_ace_syn_ect0++;
					tp->t_client_accecn_state = tcp_connection_client_accurate_ecn_negotiation_success_ect_mangling_detected;
					break;
				case (TH_AE | TH_CWR | 0):
					/* CE SYN was delivered */
					tp->ecn_flags |= ecn_flags;
					/* Mangling detected, set Non-ECT on outgoing packets */
					tp->t_client_accecn_state = tcp_connection_client_accurate_ecn_negotiation_success_ect_mangling_detected;
					tp->ecn_flags &= ~TE_SENDIPECT;
					/*
					 * Although we don't send ECT SYN yet, it is possible that
					 * a network element changed Not-ECT to ECT and later there
					 * was congestion at another network element that set it to CE.
					 * To keep it simple, we will consider this as a congestion event
					 * for the congestion controller.
					 * If a TCP client in AccECN mode receives CE feedback in the TCP
					 * flags of a SYN/ACK, it MUST NOT increment s.cep.
					 */
					tp->snd_cwnd = 2 * tp->t_maxseg;
					tcpstat.tcps_ecn_ace_syn_ce++;
					break;
				default:
					break;
				}
				if (TCP_ECN_ENABLED(tp)) {
					tcp_heuristic_ecn_success(tp);
					tcpstat.tcps_ecn_client_success++;
				}
				/*
				 * A TCP client in AccECN mode MUST feed back which of the 4
				 * possible values of the IP-ECN field that was received in the
				 * SYN/ACK. Set the setup flag for final ACK accordingly.
				 * We will initialize r.cep, r.e1b, r.e0b first and then increment
				 * if CE was set on the IP-ECN field of the SYN-ACK.
				 */
				tp->t_aecn.t_rcv_ce_packets = 5;
				tp->t_aecn.t_rcv_ect0_bytes = tp->t_aecn.t_rcv_ect1_bytes = 1;
				tp->t_aecn.t_rcv_ce_bytes = 0;

				/* Increment packet & byte counters based on IP-ECN */
				tcp_input_ip_ecn(tp, inp, (uint32_t)tlen, (uint32_t)segment_count, ip_ecn);

				switch (ip_ecn) {
				case IPTOS_ECN_NOTECT:
					/* Not-ECT SYN-ACK was received */
					tp->ecn_flags |= TE_ACE_SETUP_NON_ECT;
					break;
				case IPTOS_ECN_ECT1:
					/* ECT1 SYN-ACK was received */
					tp->ecn_flags |= TE_ACE_SETUP_ECT1;
					break;
				case IPTOS_ECN_ECT0:
					/* ECT0 SYN-ACK was received */
					tp->ecn_flags |= TE_ACE_SETUP_ECT0;
					break;
				case IPTOS_ECN_CE:
					tp->ecn_flags |= TE_ACE_SETUP_CE;
					break;
				}
				/* Update the time for this newly SYN-ACK packet */
				if ((to.to_flags & TOF_TS) != 0 && (to.to_tsecr != 0) &&
				    (tp->t_last_ack_tsecr == 0 || TSTMP_GEQ(to.to_tsecr, tp->t_last_ack_tsecr))) {
					tp->t_last_ack_tsecr = to.to_tsecr;
				}
			} else {
				if ((tp->ecn_flags & (TE_SETUPSENT | TE_ACE_SETUPSENT)) &&
				    tp->t_rxtshift == 0) {
					tcp_heuristic_ecn_success(tp);
					tcpstat.tcps_ecn_not_supported++;
				}
				if ((tp->ecn_flags & (TE_SETUPSENT | TE_ACE_SETUPSENT)) &&
				    tp->t_rxtshift > 0) {
					tcp_heuristic_ecn_loss(tp);
				}

				/* non-ECN-setup SYN-ACK */
				tp->ecn_flags &= ~TE_SENDIPECT;
				/*
				 * If Accurate ECN SYN was retransmitted twice and non-ECN SYN-ACK
				 * was received, then we consider it as Accurate ECN blackholing
				 */
				if ((tp->ecn_flags & TE_LOST_SYN) && tp->t_rxtshift <= 2 &&
				    tp->t_client_accecn_state == tcp_connection_client_accurate_ecn_feature_enabled) {
					tp->t_client_accecn_state = tcp_connection_client_accurate_ecn_negotiation_blackholed;
				}
				/*
				 * If SYN wasn't retransmitted twice yet, the server supports neither classic nor
				 * accurate ECN SYN-ACK. Accurate ECN should already be disabled for both half connections
				 * as TE_ACE_SETUPRECEIVED flag is not set.
				 */
				if (tp->t_client_accecn_state == tcp_connection_client_accurate_ecn_feature_enabled) {
					tp->t_client_accecn_state = tcp_connection_client_ecn_not_available;
				}
			}

			/* Do window scaling on this connection? */
			if (TCP_WINDOW_SCALE_ENABLED(tp)) {
				tp->snd_scale = tp->requested_s_scale;
				tp->rcv_scale = tp->request_r_scale;
			}

			uint32_t recwin = min(tp->rcv_wnd, TCP_MAXWIN << tp->rcv_scale);
			if (TCP_USE_RLEDBAT(tp, so) && tcp_cc_rledbat.get_rlwin != NULL) {
				/* For a LBE receiver, also use rledbat_win */
				uint32_t rledbat_win = tcp_cc_rledbat.get_rlwin(tp);
				if (rledbat_win > 0) {
					recwin = min(recwin, rledbat_win);
				}
			}
			tp->rcv_adv += recwin;

			tp->snd_una++;          /* SYN is acked */
			if (SEQ_LT(tp->snd_nxt, tp->snd_una)) {
				tp->snd_nxt = tp->snd_una;
			}

			/*
			 * We have sent more in the SYN than what is being
			 * acked. (e.g., TFO)
			 * We should restart the sending from what the receiver
			 * has acknowledged immediately.
			 */
			if (SEQ_GT(tp->snd_nxt, th->th_ack)) {
				/*
				 * rdar://problem/33214601
				 * There is a middlebox that acks all but one
				 * byte and still drops the data.
				 */
				if (!(tp->t_flagsext & TF_FASTOPEN_FORCE_ENABLE) &&
				    (tp->t_tfo_stats & TFO_S_SYN_DATA_SENT) &&
				    tp->snd_max == th->th_ack + 1 &&
				    tp->snd_max > tp->snd_una + 1) {
					tcp_heuristic_tfo_middlebox(tp);

					so->so_error = ENODATA;
					soevent(so,
					    (SO_FILT_HINT_LOCKED | SO_FILT_HINT_MP_SUB_ERROR));

					tp->t_tfo_stats |= TFO_S_ONE_BYTE_PROXY;
				}

				tp->snd_max = tp->snd_nxt = th->th_ack;
			}

			/*
			 * If there's data, delay ACK; if there's also a FIN
			 * ACKNOW will be turned on later.
			 */
			TCP_INC_VAR(tp->t_unacksegs, segment_count);
			if (TCP_ACC_ECN_ON(tp) && ip_ecn == IPTOS_ECN_CE) {
				TCP_INC_VAR(tp->t_unacksegs_ce, segment_count);
			}
			if (DELAY_ACK(tp, th) && tlen != 0) {
				if ((tp->t_flags & TF_DELACK) == 0) {
					tp->t_flags |= TF_DELACK;
					tp->t_timer[TCPT_DELACK] = OFFSET_FROM_START(tp, tcp_delack);
				}
			} else {
				tp->t_flags |= TF_ACKNOW;
			}
			/*
			 * Received <SYN,ACK> in SYN_SENT[*] state.
			 * Transitions:
			 *	SYN_SENT  --> ESTABLISHED
			 *	SYN_SENT* --> FIN_WAIT_1
			 */
			tp->t_starttime = tcp_now;
			tcp_sbrcv_tstmp_check(tp);
			if (tp->t_flags & TF_NEEDFIN) {
				DTRACE_TCP4(state__change, void, NULL,
				    struct inpcb *, inp,
				    struct tcpcb *, tp, int32_t,
				    TCPS_FIN_WAIT_1);
				TCP_LOG_STATE(tp, TCPS_FIN_WAIT_1);
				tp->t_state = TCPS_FIN_WAIT_1;
				tp->t_flags &= ~TF_NEEDFIN;
				thflags &= ~TH_SYN;

				TCP_LOG_CONNECTION_SUMMARY(tp);
			} else {
				DTRACE_TCP4(state__change, void, NULL,
				    struct inpcb *, inp, struct tcpcb *,
				    tp, int32_t, TCPS_ESTABLISHED);
				TCP_LOG_STATE(tp, TCPS_ESTABLISHED);
				tp->t_state = TCPS_ESTABLISHED;
				tp->t_timer[TCPT_KEEP] =
				    OFFSET_FROM_START(tp,
				    TCP_CONN_KEEPIDLE(tp));
				if (nstat_collect) {
					nstat_route_connect_success(
						inp->inp_route.ro_rt);
				}
				TCP_LOG_CONNECTED(tp, 0);
				/*
				 * The SYN is acknowledged but una is not
				 * updated yet. So pass the value of
				 * ack to compute sndbytes correctly
				 */
				inp_count_sndbytes(inp, th->th_ack);
			}
			tp->t_forced_acks = TCP_FORCED_ACKS_COUNT;
#if MPTCP
			/*
			 * Do not send the connect notification for additional
			 * subflows until ACK for 3-way handshake arrives.
			 */
			if ((!(tp->t_mpflags & TMPF_MPTCP_TRUE)) &&
			    (tp->t_mpflags & TMPF_SENT_JOIN)) {
				isconnected = FALSE;
			} else
#endif /* MPTCP */
			isconnected = TRUE;

			if ((tp->t_tfo_flags & (TFO_F_COOKIE_REQ | TFO_F_COOKIE_SENT)) ||
			    (tp->t_tfo_stats & TFO_S_SYN_DATA_SENT)) {
				tcp_tfo_synack(tp, &to);

				if ((tp->t_tfo_stats & TFO_S_SYN_DATA_SENT) &&
				    SEQ_LT(tp->snd_una, th->th_ack)) {
					tp->t_tfo_stats |= TFO_S_SYN_DATA_ACKED;
					tcpstat.tcps_tfo_syn_data_acked++;
#if MPTCP
					if (so->so_flags & SOF_MP_SUBFLOW) {
						so->so_flags1 |= SOF1_TFO_REWIND;
					}
#endif
					tcp_tfo_rcv_probe(tp, tlen);
				}
			}
		} else {
			/*
			 *  Received initial SYN in SYN-SENT[*] state => simul-
			 *  taneous open.
			 *  Do 3-way handshake:
			 *        SYN-SENT -> SYN-RECEIVED
			 *        SYN-SENT* -> SYN-RECEIVED*
			 */
			tp->t_flags |= TF_ACKNOW;
			tp->t_timer[TCPT_REXMT] = 0;
			DTRACE_TCP4(state__change, void, NULL, struct inpcb *, inp,
			    struct tcpcb *, tp, int32_t, TCPS_SYN_RECEIVED);
			TCP_LOG_STATE(tp, TCPS_SYN_RECEIVED);
			tp->t_state = TCPS_SYN_RECEIVED;

			/*
			 * During simultaneous open, TFO should not be used.
			 * So, we disable it here, to prevent that data gets
			 * sent on the SYN/ACK.
			 */
			tcp_disable_tfo(tp);
		}

trimthenstep6:
		/*
		 * Advance th->th_seq to correspond to first data byte.
		 * If data, trim to stay within window,
		 * dropping FIN if necessary.
		 */
		th->th_seq++;
		if (tlen > tp->rcv_wnd) {
			todrop = tlen - tp->rcv_wnd;
			m_adj(m, -todrop);
			tlen = tp->rcv_wnd;
			thflags &= ~TH_FIN;
			tcpstat.tcps_rcvpackafterwin++;
			tcpstat.tcps_rcvbyteafterwin += todrop;
		}
		tp->snd_wl1 = th->th_seq - 1;
		tp->rcv_up = th->th_seq;
		/*
		 *  Client side of transaction: already sent SYN and data.
		 *  If the remote host used T/TCP to validate the SYN,
		 *  our data will be ACK'd; if so, enter normal data segment
		 *  processing in the middle of step 5, ack processing.
		 *  Otherwise, goto step 6.
		 */
		if (thflags & TH_ACK) {
			goto process_ACK;
		}
		goto step6;
	/*
	 * If the state is LAST_ACK or CLOSING or TIME_WAIT:
	 *      do normal processing.
	 *
	 * NB: Leftover from RFC1644 T/TCP.  Cases to be reused later.
	 */
	case TCPS_LAST_ACK:
	case TCPS_CLOSING:
	case TCPS_TIME_WAIT:
		break;  /* continue normal processing */

	/* Received a SYN while connection is already established.
	 * This is a "half open connection and other anomalies" described
	 * in RFC793 page 34, send an ACK so the remote reset the connection
	 * or recovers by adjusting its sequence numbering. Sending an ACK is
	 * in accordance with RFC 5961 Section 4.2
	 *
	 * For Accurate ECN, if we receive a packet with SYN in ESTABLISHED
	 * state, we don't send the handshake encoding.
	 */
	case TCPS_ESTABLISHED:
		if (thflags & TH_SYN && tlen <= 0) {
			/* Drop the packet silently if we have reached the limit */
			if (tcp_is_ack_ratelimited(tp)) {
				TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "ESTABLISHED rfc5961 rate limited");
				goto drop;
			} else {
				/* Send challenge ACK */
				tcpstat.tcps_synchallenge++;
				TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "ESTABLISHED rfc5961 challenge ACK");
				goto dropafterack;
			}
		}
		break;
	}

	/*
	 * States other than LISTEN or SYN_SENT.
	 * First check the RST flag and sequence number since reset segments
	 * are exempt from the timestamp and connection count tests.  This
	 * fixes a bug introduced by the Stevens, vol. 2, p. 960 bugfix
	 * below which allowed reset segments in half the sequence space
	 * to fall though and be processed (which gives forged reset
	 * segments with a random sequence number a 50 percent chance of
	 * killing a connection).
	 * Then check timestamp, if present.
	 * Then check the connection count, if present.
	 * Then check that at least some bytes of segment are within
	 * receive window.  If segment begins before rcv_nxt,
	 * drop leading data (and SYN); if nothing left, just ack.
	 *
	 *
	 * If the RST bit is set, check the sequence number to see
	 * if this is a valid reset segment.
	 * RFC 793 page 37:
	 *   In all states except SYN-SENT, all reset (RST) segments
	 *   are validated by checking their SEQ-fields.  A reset is
	 *   valid if its sequence number is in the window.
	 * Note: this does not take into account delayed ACKs, so
	 *   we should test against last_ack_sent instead of rcv_nxt.
	 *   The sequence number in the reset segment is normally an
	 *   echo of our outgoing acknowlegement numbers, but some hosts
	 *   send a reset with the sequence number at the rightmost edge
	 *   of our receive window, and we have to handle this case.
	 * Note 2: Paul Watson's paper "Slipping in the Window" has shown
	 *   that brute force RST attacks are possible.  To combat this,
	 *   we use a much stricter check while in the ESTABLISHED state,
	 *   only accepting RSTs where the sequence number is equal to
	 *   last_ack_sent.  In all other states (the states in which a
	 *   RST is more likely), the more permissive check is used.
	 * RFC 5961 Section 3.2: if the RST bit is set, sequence # is
	 *    within the receive window and last_ack_sent == seq,
	 *    then reset the connection. Otherwise if the seq doesn't
	 *    match last_ack_sent, TCP must send challenge ACK. Perform
	 *    rate limitation when sending the challenge ACK.
	 * If we have multiple segments in flight, the intial reset
	 * segment sequence numbers will be to the left of last_ack_sent,
	 * but they will eventually catch up.
	 * In any case, it never made sense to trim reset segments to
	 * fit the receive window since RFC 1122 says:
	 *   4.2.2.12  RST Segment: RFC-793 Section 3.4
	 *
	 *    A TCP SHOULD allow a received RST segment to include data.
	 *
	 *    DISCUSSION
	 *         It has been suggested that a RST segment could contain
	 *         ASCII text that encoded and explained the cause of the
	 *         RST.  No standard has yet been established for such
	 *         data.
	 *
	 * If the reset segment passes the sequence number test examine
	 * the state:
	 *    SYN_RECEIVED STATE:
	 *	If passive open, return to LISTEN state.
	 *	If active open, inform user that connection was refused.
	 *    ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT STATES:
	 *	Inform user that connection was reset, and close tcb.
	 *    CLOSING, LAST_ACK STATES:
	 *	Close the tcb.
	 *    TIME_WAIT STATE:
	 *	Drop the segment - see Stevens, vol. 2, p. 964 and
	 *      RFC 1337.
	 *
	 *      Radar 4803931: Allows for the case where we ACKed the FIN but
	 *                     there is already a RST in flight from the peer.
	 *                     In that case, accept the RST for non-established
	 *                     state if it's one off from last_ack_sent.
	 *
	 */
	if (thflags & TH_RST) {
		if ((SEQ_GEQ(th->th_seq, tp->last_ack_sent) &&
		    SEQ_LT(th->th_seq, tp->last_ack_sent + tp->rcv_wnd)) ||
		    (tp->rcv_wnd == 0 &&
		    ((tp->last_ack_sent == th->th_seq) ||
		    ((tp->last_ack_sent - 1) == th->th_seq)))) {
			if (tp->last_ack_sent == th->th_seq) {
				switch (tp->t_state) {
				case TCPS_SYN_RECEIVED:
					IF_TCP_STATINC(ifp, rstinsynrcv);
					so->so_error = ECONNREFUSED;
					goto close;

				case TCPS_ESTABLISHED:
					if ((TCP_ECN_ENABLED(tp) || TCP_ACC_ECN_ON(tp)) &&
					    tp->snd_una == tp->iss + 1 &&
					    SEQ_GT(tp->snd_max, tp->snd_una)) {
						/*
						 * If the first data packet on an
						 * ECN connection, receives a RST
						 * increment the heuristic
						 */
						tcp_heuristic_ecn_droprst(tp);
					}
					OS_FALLTHROUGH;
				case TCPS_FIN_WAIT_1:
				case TCPS_CLOSE_WAIT:
				case TCPS_FIN_WAIT_2:
					so->so_error = ECONNRESET;
close:
					soevent(so,
					    (SO_FILT_HINT_LOCKED |
					    SO_FILT_HINT_CONNRESET));

					tcpstat.tcps_drops++;
					tp = tcp_close(tp);
					break;

				case TCPS_CLOSING:
				case TCPS_LAST_ACK:
					tp = tcp_close(tp);
					break;

				case TCPS_TIME_WAIT:
					break;
				}
			} else {
				tcpstat.tcps_badrst++;
				/* Drop if we have reached the ACK limit */
				if (tcp_is_ack_ratelimited(tp)) {
					TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "ESTABLISHED rfc5961 rate limited");
					goto drop;
				} else {
					/* Send challenge ACK */
					tcpstat.tcps_rstchallenge++;
					TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "ESTABLISHED rfc5961 challenge ACK");
					goto dropafterack;
				}
			}
		}
		goto drop;
	}

	/*
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment
	 * and it's less than ts_recent, drop it.
	 */
	if ((to.to_flags & TOF_TS) != 0 && tp->ts_recent &&
	    TSTMP_LT(to.to_tsval, tp->ts_recent)) {
		/* Check to see if ts_recent is over 24 days old.  */
		if ((int)(tcp_now - tp->ts_recent_age) > TCP_PAWS_IDLE) {
			/*
			 * Invalidate ts_recent.  If this segment updates
			 * ts_recent, the age will be reset later and ts_recent
			 * will get a valid value.  If it does not, setting
			 * ts_recent to zero will at least satisfy the
			 * requirement that zero be placed in the timestamp
			 * echo reply when ts_recent isn't valid.  The
			 * age isn't reset until we get a valid ts_recent
			 * because we don't want out-of-order segments to be
			 * dropped when ts_recent is old.
			 */
			tp->ts_recent = 0;
		} else {
			tcpstat.tcps_rcvduppack++;
			tcpstat.tcps_rcvdupbyte += tlen;
			tp->t_pawsdrop++;
			tcpstat.tcps_pawsdrop++;

			/*
			 * PAWS-drop when ECN is being used? That indicates
			 * that ECT-marked packets take a different path, with
			 * different congestion-characteristics.
			 *
			 * Only fallback when we did send less than 2GB as PAWS
			 * really has no reason to kick in earlier.
			 */
			if ((TCP_ECN_ENABLED(tp) || TCP_ACC_ECN_ON(tp)) &&
			    inp->inp_stat->rxbytes < 2147483648) {
				INP_INC_IFNET_STAT(inp, ecn_fallback_reorder);
				tcpstat.tcps_ecn_fallback_reorder++;
				tcp_heuristic_ecn_aggressive(tp);
			}

			if (nstat_collect) {
				nstat_route_rx(tp->t_inpcb->inp_route.ro_rt,
				    1, tlen, NSTAT_RX_FLAG_DUPLICATE);
				INP_ADD_STAT(inp, ifnet_count_type,
				    rxpackets, 1);
				INP_ADD_STAT(inp, ifnet_count_type,
				    rxbytes, tlen);
				tp->t_stat.rxduplicatebytes += tlen;
				inp_set_activity_bitmap(inp);
			}
			if (tlen > 0) {
				goto dropafterack;
			}
			goto drop;
		}
	}

	/*
	 * In the SYN-RECEIVED state, validate that the packet belongs to
	 * this connection before trimming the data to fit the receive
	 * window.  Check the sequence number versus IRS since we know
	 * the sequence numbers haven't wrapped.  This is a partial fix
	 * for the "LAND" DoS attack.
	 */
	if (tp->t_state == TCPS_SYN_RECEIVED && SEQ_LT(th->th_seq, tp->irs)) {
		IF_TCP_STATINC(ifp, dospacket);
		TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "SYN_RECEIVED bad SEQ");
		goto dropwithreset;
	}

	/*
	 * Check if there is old data at the beginning of the window
	 * i.e. the sequence number is before rcv_nxt
	 */
	todrop = tp->rcv_nxt - th->th_seq;
	if (todrop > 0) {
		boolean_t is_syn_set = FALSE;

		if (thflags & TH_SYN) {
			is_syn_set = TRUE;
			thflags &= ~TH_SYN;
			th->th_seq++;
			if (th->th_urp > 1) {
				th->th_urp--;
			} else {
				thflags &= ~TH_URG;
			}
			todrop--;
		}
		/*
		 * Following if statement from Stevens, vol. 2, p. 960.
		 * The amount of duplicate data is greater than or equal
		 * to the size of the segment - entire segment is duplicate
		 */
		if (todrop > tlen
		    || (todrop == tlen && (thflags & TH_FIN) == 0)) {
			/*
			 * Any valid FIN must be to the left of the window.
			 * At this point the FIN must be a duplicate or out
			 * of sequence; drop it.
			 */
			thflags &= ~TH_FIN;

			/*
			 * Send an ACK to resynchronize and drop any data.
			 * But keep on processing for RST or ACK.
			 *
			 * If the SYN bit was originally set, then only send
			 * an ACK if we are not rate-limiting this connection.
			 */
			if (is_syn_set) {
				if (!tcp_is_ack_ratelimited(tp)) {
					tcpstat.tcps_synchallenge++;
					tp->t_flags |= TF_ACKNOW;
				}
			} else {
				tp->t_flags |= TF_ACKNOW;
			}

			if (todrop == 1) {
				/* This could be a keepalive */
				soevent(so, SO_FILT_HINT_LOCKED |
				    SO_FILT_HINT_KEEPALIVE);
			}
			todrop = tlen;
			tcpstat.tcps_rcvduppack++;
			tcpstat.tcps_rcvdupbyte += todrop;
		} else {
			tcpstat.tcps_rcvpartduppack++;
			tcpstat.tcps_rcvpartdupbyte += todrop;
		}

		if (todrop > 1) {
			/*
			 * Note the duplicate data sequence space so that
			 * it can be reported in DSACK option.
			 */
			tp->t_dsack_lseq = th->th_seq;
			tp->t_dsack_rseq = th->th_seq + todrop;
			tp->t_flags |= TF_ACKNOW;
		}
		if (nstat_collect) {
			nstat_route_rx(tp->t_inpcb->inp_route.ro_rt, 1,
			    todrop, NSTAT_RX_FLAG_DUPLICATE);
			INP_ADD_STAT(inp, ifnet_count_type, rxpackets, 1);
			INP_ADD_STAT(inp, ifnet_count_type, rxbytes, todrop);
			tp->t_stat.rxduplicatebytes += todrop;
			inp_set_activity_bitmap(inp);
		}
		drop_hdrlen += todrop;  /* drop from the top afterwards */
		th->th_seq += todrop;
		tlen -= todrop;
		if (th->th_urp > todrop) {
			th->th_urp -= todrop;
		} else {
			thflags &= ~TH_URG;
			th->th_urp = 0;
		}
	}

	/*
	 * If new data are received on a connection after the user
	 * processes are gone, then RST the other end.
	 * Send also a RST when we received a data segment after we've
	 * sent our FIN when the socket is defunct.
	 * Note that an MPTCP subflow socket would have SS_NOFDREF set
	 * by default. So, if it's an MPTCP-subflow we rather check the
	 * MPTCP-level's socket state for SS_NOFDREF.
	 */
	if (tlen) {
		boolean_t close_it = FALSE;

		if (!(so->so_flags & SOF_MP_SUBFLOW) && (so->so_state & SS_NOFDREF) &&
		    tp->t_state > TCPS_CLOSE_WAIT) {
			TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "SS_NOFDREF");
			close_it = TRUE;
		}

		if ((so->so_flags & SOF_MP_SUBFLOW) && (mptetoso(tptomptp(tp)->mpt_mpte)->so_state & SS_NOFDREF) &&
		    tp->t_state > TCPS_CLOSE_WAIT) {
			TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "SOF_MP_SUBFLOW SS_NOFDREF");
			close_it = TRUE;
		}

		if ((so->so_flags & SOF_DEFUNCT) && tp->t_state > TCPS_FIN_WAIT_1) {
			TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "SOF_DEFUNCT");
			close_it = TRUE;
		}

		if (so->so_state & SS_CANTRCVMORE) {
			TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "SS_CANTRCVMORE");
			close_it = TRUE;
		}

		if (close_it) {
			tp = tcp_close(tp);
			tcpstat.tcps_rcvafterclose++;
			IF_TCP_STATINC(ifp, cleanup);
			goto dropwithreset;
		}
	}

	/*
	 * If segment ends after window, drop trailing data
	 * (and PUSH and FIN); if nothing left, just ACK.
	 */
	todrop = (th->th_seq + tlen) - (tp->rcv_nxt + tp->rcv_wnd);
	if (todrop > 0) {
		tcpstat.tcps_rcvpackafterwin++;
		if (todrop >= tlen) {
			tcpstat.tcps_rcvbyteafterwin += tlen;
			/*
			 * If a new connection request is received
			 * while in TIME_WAIT, drop the old connection
			 * and start over if the sequence numbers
			 * are above the previous ones.
			 */
			if (thflags & TH_SYN &&
			    tp->t_state == TCPS_TIME_WAIT &&
			    SEQ_GT(th->th_seq, tp->rcv_nxt)) {
				iss = tcp_new_isn(tp);
				tp = tcp_close(tp);
				socket_unlock(so, 1);
				goto findpcb;
			}
			/*
			 * If window is closed can only take segments at
			 * window edge, and have to drop data and PUSH from
			 * incoming segments.  Continue processing, but
			 * remember to ack.  Otherwise, drop segment
			 * and ack.
			 */
			if (tp->rcv_wnd == 0 && th->th_seq == tp->rcv_nxt) {
				tp->t_flags |= TF_ACKNOW;
				tcpstat.tcps_rcvwinprobe++;
			} else {
				goto dropafterack;
			}
		} else {
			tcpstat.tcps_rcvbyteafterwin += todrop;
		}
		m_adj(m, -todrop);
		tlen -= todrop;
		thflags &= ~(TH_PUSH | TH_FIN);
	}

	/*
	 * If last ACK falls within this segment's sequence numbers,
	 * record its timestamp.
	 * NOTE:
	 * 1) That the test incorporates suggestions from the latest
	 *    proposal of the tcplw@cray.com list (Braden 1993/04/26).
	 * 2) That updating only on newer timestamps interferes with
	 *    our earlier PAWS tests, so this check should be solely
	 *    predicated on the sequence space of this segment.
	 * 3) That we modify the segment boundary check to be
	 *        Last.ACK.Sent <= SEG.SEQ + SEG.Len
	 *    instead of RFC1323's
	 *        Last.ACK.Sent < SEG.SEQ + SEG.Len,
	 *    This modified check allows us to overcome RFC1323's
	 *    limitations as described in Stevens TCP/IP Illustrated
	 *    Vol. 2 p.869. In such cases, we can still calculate the
	 *    RTT correctly when RCV.NXT == Last.ACK.Sent.
	 */
	if ((to.to_flags & TOF_TS) != 0 &&
	    SEQ_LEQ(th->th_seq, tp->last_ack_sent) &&
	    SEQ_LEQ(tp->last_ack_sent, th->th_seq + tlen +
	    ((thflags & (TH_SYN | TH_FIN)) != 0))) {
		tp->ts_recent_age = tcp_now;
		tp->ts_recent = to.to_tsval;
	}

	/*
	 * Stevens: If a SYN is in the window, then this is an
	 * error and we send an RST and drop the connection.
	 *
	 * RFC 5961 Section 4.2
	 * Send challenge ACK for any SYN in synchronized state
	 * Perform rate limitation in doing so.
	 */
	if (thflags & TH_SYN) {
		if (!tcp_syn_data_valid(tp, th, tlen)) {
			tcpstat.tcps_badsyn++;
			/* Drop if we have reached ACK limit */
			if (tcp_is_ack_ratelimited(tp)) {
				TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "rfc5961 bad SYN rate limited");
				goto drop;
			} else {
				/* Send challenge ACK */
				tcpstat.tcps_synchallenge++;
				TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "rfc5961 bad SYN challenge ack");
				goto dropafterack;
			}
		} else {
			/*
			 * Received SYN (/ACK) with data.
			 * Move sequence number along to process the data.
			 */
			th->th_seq++;
			thflags &= ~TH_SYN;
		}
	}

	/*
	 * If the ACK bit is off:  if in SYN-RECEIVED state or SENDSYN
	 * flag is on (half-synchronized state), then queue data for
	 * later processing; else drop segment and return.
	 */
	if ((thflags & TH_ACK) == 0) {
		if (tp->t_state == TCPS_SYN_RECEIVED) {
			if ((TFO_ENABLED(tp))) {
				/*
				 * So, we received a valid segment while in
				 * SYN-RECEIVED.
				 * As this cannot be an RST (see that if a bit
				 * higher), and it does not have the ACK-flag
				 * set, we want to retransmit the SYN/ACK.
				 * Thus, we have to reset snd_nxt to snd_una to
				 * trigger the going back to sending of the
				 * SYN/ACK. This is more consistent with the
				 * behavior of tcp_output(), which expects
				 * to send the segment that is pointed to by
				 * snd_nxt.
				 */
				tp->snd_nxt = tp->snd_una;

				/*
				 * We need to make absolutely sure that we are
				 * going to reply upon a duplicate SYN-segment.
				 */
				if (th->th_flags & TH_SYN) {
					needoutput = 1;
				}
			}
			/* Process this same as newly received Accurate ECN SYN */
			int ace_flags = ((th->th_x2 << 8) | thflags) & TH_ACE;
			tcp_input_process_accecn_syn(tp, ace_flags, ip_ecn);

			goto step6;
		} else if (tp->t_flags & TF_ACKNOW) {
			TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "bad ACK");
			goto dropafterack;
		} else {
			TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "bad ACK");
			goto drop;
		}
	}

	/*
	 * Ack processing.
	 */

	switch (tp->t_state) {
	/*
	 * In SYN_RECEIVED state, the ack ACKs our SYN, so enter
	 * ESTABLISHED state and continue processing.
	 * The ACK was checked above.
	 */
	case TCPS_SYN_RECEIVED:

		tcpstat.tcps_connects++;

		/* Do window scaling? */
		if (TCP_WINDOW_SCALE_ENABLED(tp)) {
			tp->snd_scale = tp->requested_s_scale;
			tp->rcv_scale = tp->request_r_scale;
			tp->snd_wnd = th->th_win << tp->snd_scale;
			tp->max_sndwnd = tp->snd_wnd;
			tiwin = tp->snd_wnd;
		}
		/*
		 * Make transitions:
		 *      SYN-RECEIVED  -> ESTABLISHED
		 *      SYN-RECEIVED* -> FIN-WAIT-1
		 */
		tp->t_starttime = tcp_now;
		tcp_sbrcv_tstmp_check(tp);
		if (tp->t_flags & TF_NEEDFIN) {
			DTRACE_TCP4(state__change, void, NULL,
			    struct inpcb *, inp,
			    struct tcpcb *, tp, int32_t, TCPS_FIN_WAIT_1);
			TCP_LOG_STATE(tp, TCPS_FIN_WAIT_1);
			tp->t_state = TCPS_FIN_WAIT_1;
			tp->t_flags &= ~TF_NEEDFIN;

			TCP_LOG_CONNECTION_SUMMARY(tp);
		} else {
			DTRACE_TCP4(state__change, void, NULL,
			    struct inpcb *, inp,
			    struct tcpcb *, tp, int32_t, TCPS_ESTABLISHED);
			TCP_LOG_STATE(tp, TCPS_ESTABLISHED);
			tp->t_state = TCPS_ESTABLISHED;
			tp->t_timer[TCPT_KEEP] = OFFSET_FROM_START(tp,
			    TCP_CONN_KEEPIDLE(tp));
			if (nstat_collect) {
				nstat_route_connect_success(
					tp->t_inpcb->inp_route.ro_rt);
			}
			TCP_LOG_CONNECTED(tp, 0);
			/*
			 * The SYN is acknowledged but una is not updated
			 * yet. So pass the value of ack to compute
			 * sndbytes correctly
			 */
			inp_count_sndbytes(inp, th->th_ack);
		}
		tp->t_forced_acks = TCP_FORCED_ACKS_COUNT;

		VERIFY(LIST_EMPTY(&tp->t_segq));
		tp->snd_wl1 = th->th_seq - 1;

		/*
		 * AccECN server in SYN-RCVD state received an ACK with
		 * SYN=0, process handshake encoding present in the ACK for SYN-ACK
		 * and update receive side counters.
		 */
		if (TCP_ACC_ECN_ON(tp) && (thflags & (TH_SYN | TH_ACK)) == TH_ACK) {
			const uint32_t ace_flags = ((th->th_x2 << 8) | thflags) & TH_ACE;
			if (tlen == 0 && to.to_nsacks == 0) {
				/*
				 * ACK for SYN-ACK reflects the state (ECN) in which SYN-ACK packet
				 * was delivered. Use Table 4 of Accurate ECN draft to decode only
				 * when a pure ACK with no SACK block is received.
				 * 0|0|0 will fail Accurate ECN negotiation and disable ECN.
				 */
				switch (ace_flags) {
				case (0 | TH_CWR | 0):
					/* Non-ECT SYN-ACK was delivered */
					tp->t_aecn.t_snd_ce_packets = 5;
					if (tp->t_server_accecn_state == tcp_connection_server_accurate_ecn_requested) {
						tp->t_server_accecn_state = tcp_connection_server_accurate_ecn_negotiation_success;
					}
					break;
				case (0 | TH_CWR | TH_ECE):
					/* ECT1 SYN-ACK was delivered, mangling detected */
					OS_FALLTHROUGH;
				case (TH_AE | 0 | 0):
					/* ECT0 SYN-ACK was delivered, mangling detected */
					tp->t_aecn.t_snd_ce_packets = 5;
					if (tp->t_server_accecn_state == tcp_connection_server_accurate_ecn_requested) {
						tp->t_server_accecn_state = tcp_connection_server_accurate_ecn_negotiation_success_ect_mangling_detected;
					}
					break;
				case (TH_AE | TH_CWR | 0):
					/*
					 * CE SYN-ACK was delivered, even though mangling happened,
					 * CE could indicate congestion at a node after mangling occured.
					 * Set cwnd to 2 segments
					 */
					tp->t_aecn.t_snd_ce_packets = 6;
					tp->snd_cwnd = 2 * tp->t_maxseg;
					if (tp->t_server_accecn_state == tcp_connection_server_accurate_ecn_requested) {
						tp->t_server_accecn_state = tcp_connection_server_accurate_ecn_negotiation_success_ect_mangling_detected;
					}
					break;
				case (0 | 0 | 0):
					/* Disable ECN, as ACE fields were zeroed */
					tp->ecn_flags &= ~(TE_SETUPRECEIVED | TE_SENDIPECT |
					    TE_SENDCWR | TE_ACE_SETUPRECEIVED);
					/*
					 * Since last ACK has no ECN flag set and TE_LOST_SYNACK is set, this is in response
					 * to the second (non-ECN setup) SYN-ACK retransmission. In such a case, we assume
					 * that AccECN SYN-ACK was blackholed.
					 */
					if ((tp->ecn_flags & TE_LOST_SYNACK) && tp->t_rxtshift <= 2 &&
					    (tp->t_server_accecn_state == tcp_connection_server_classic_ecn_requested ||
					    tp->t_server_accecn_state == tcp_connection_server_accurate_ecn_requested)) {
						tp->t_server_accecn_state = tcp_connection_server_accurate_ecn_negotiation_blackholed;
					}
					/*
					 * SYN-ACK hasn't been retransmitted twice yet, so this could likely mean bleaching of ACE
					 * on the path from client to server on last ACK.
					 */
					if (tp->t_server_accecn_state == tcp_connection_server_accurate_ecn_requested) {
						tp->t_server_accecn_state = tcp_connection_server_accurate_ecn_ace_bleaching_detected;
					}
					break;
				default:
					/* Unused values for forward compatibility */
					tp->t_aecn.t_snd_ce_packets = 5;
					break;
				}
				/* Update the time for this newly received last ACK */
				if ((to.to_flags & TOF_TS) != 0 && (to.to_tsecr != 0) &&
				    (tp->t_last_ack_tsecr == 0 || TSTMP_GEQ(to.to_tsecr, tp->t_last_ack_tsecr))) {
					tp->t_last_ack_tsecr = to.to_tsecr;
				}
			} else if (to.to_nsacks == 0) {
				/*
				 * If 3rd ACK is lost, we won't receive the last ACK
				 * encoding. We will move the server to AccECN mode
				 * regardless.
				 */
				tp->t_aecn.t_snd_ce_packets = 5;
				if (tp->t_server_accecn_state == tcp_connection_server_accurate_ecn_requested) {
					tp->t_server_accecn_state = tcp_connection_server_accurate_ecn_negotiation_success;
				}
			}
			/* Increment receive side counters based on IP-ECN */
			tcp_input_ip_ecn(tp, inp, (uint32_t)tlen, (uint32_t)segment_count, ip_ecn);
		}

#if MPTCP
		/*
		 * Do not send the connect notification for additional subflows
		 * until ACK for 3-way handshake arrives.
		 */
		if ((!(tp->t_mpflags & TMPF_MPTCP_TRUE)) &&
		    (tp->t_mpflags & TMPF_SENT_JOIN)) {
			isconnected = FALSE;
		} else
#endif /* MPTCP */
		isconnected = TRUE;
		if ((tp->t_tfo_flags & TFO_F_COOKIE_VALID)) {
			/* Done this when receiving the SYN */
			isconnected = FALSE;

			OSDecrementAtomic(&tcp_tfo_halfcnt);

			/* Panic if something has gone terribly wrong. */
			VERIFY(tcp_tfo_halfcnt >= 0);

			tp->t_tfo_flags &= ~TFO_F_COOKIE_VALID;
		}

		/*
		 * In case there is data in the send-queue (e.g., TFO is being
		 * used, or connectx+data has been done), then if we would
		 * "FALLTHROUGH", we would handle this ACK as if data has been
		 * acknowledged. But, we have to prevent this. And this
		 * can be prevented by increasing snd_una by 1, so that the
		 * SYN is not considered as data (snd_una++ is actually also
		 * done in SYN_SENT-state as part of the regular TCP stack).
		 *
		 * In case there is data on this ack as well, the data will be
		 * handled by the label "dodata" right after step6.
		 */
		if (so->so_snd.sb_cc) {
			tp->snd_una++;  /* SYN is acked */
			if (SEQ_LT(tp->snd_nxt, tp->snd_una)) {
				tp->snd_nxt = tp->snd_una;
			}

			/*
			 * No duplicate-ACK handling is needed. So, we
			 * directly advance to processing the ACK (aka,
			 * updating the RTT estimation,...)
			 *
			 * But, we first need to handle eventual SACKs,
			 * because TFO will start sending data with the
			 * SYN/ACK, so it might be that the client
			 * includes a SACK with its ACK.
			 */
			if (SACK_ENABLED(tp) &&
			    (to.to_nsacks > 0 || !TAILQ_EMPTY(&tp->snd_holes))) {
				tcp_sack_doack(tp, &to, th, &sack_bytes_acked, &highest_sacked_seq);
			}

			goto process_ACK;
		}

		OS_FALLTHROUGH;

	/*
	 * In ESTABLISHED state: drop duplicate ACKs; ACK out of range
	 * ACKs.  If the ack is in the range
	 *	tp->snd_una < th->th_ack <= tp->snd_max
	 * then advance tp->snd_una to th->th_ack and drop
	 * data from the retransmission queue.  If this ACK reflects
	 * more up to date window information we update our window information.
	 */
	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_FIN_WAIT_2:
	case TCPS_CLOSE_WAIT:
	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
	case TCPS_TIME_WAIT:
		if (SEQ_GT(th->th_ack, tp->snd_max)) {
			tcpstat.tcps_rcvacktoomuch++;
			if (tcp_is_ack_ratelimited(tp)) {
				TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "rfc5961 rcvacktoomuch");
				goto drop;
			} else {
				goto dropafterack;
			}
		}
		if (SEQ_LT(th->th_ack, tp->snd_una - tp->max_sndwnd)) {
			if (tcp_is_ack_ratelimited(tp)) {
				TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "rfc5961 bad ACK");
				goto drop;
			} else {
				goto dropafterack;
			}
		}
		if (SACK_ENABLED(tp) && to.to_nsacks > 0) {
			recvd_dsack = tcp_sack_process_dsack(tp, &to, th, &dsack_tlp);
			/*
			 * If DSACK is received and this packet has no
			 * other SACK information, it can be dropped.
			 * We do not want to treat it as a duplicate ack.
			 */
			if (recvd_dsack &&
			    SEQ_LEQ(th->th_ack, tp->snd_una) &&
			    to.to_nsacks == 0) {
				tcp_bad_rexmt_check(tp, th, &to);
				goto drop;
			}
		}

		if (SACK_ENABLED(tp) &&
		    (to.to_nsacks > 0 || !TAILQ_EMPTY(&tp->snd_holes))) {
			tcp_sack_doack(tp, &to, th, &sack_bytes_acked, &highest_sacked_seq);
		}

		if (TCP_RACK_ENABLED(tp)) {
			/* If DSACK was received (not due to TLP), then update the reordering window */
			if (recvd_dsack && !dsack_tlp) {
				tp->rack.dsack_round_seen = 1;
			}
			tcp_rack_update_reordering_window(tp, th->th_ack);
		}

#if MPTCP
		if (tp->t_mpuna && SEQ_GEQ(th->th_ack, tp->t_mpuna)) {
			if (tp->t_mpflags & TMPF_PREESTABLISHED) {
				/* MP TCP establishment succeeded */
				tp->t_mpuna = 0;
				if (tp->t_mpflags & TMPF_JOINED_FLOW) {
					if (tp->t_mpflags & TMPF_SENT_JOIN) {
						tp->t_mpflags &=
						    ~TMPF_PREESTABLISHED;
						tp->t_mpflags |=
						    TMPF_MPTCP_TRUE;

						tp->t_timer[TCPT_JACK_RXMT] = 0;
						tp->t_mprxtshift = 0;
						isconnected = TRUE;
					} else {
						isconnected = FALSE;
					}
				} else {
					isconnected = TRUE;
				}
			}
		}
#endif /* MPTCP */

		tcp_tfo_rcv_ack(tp, th);

		/*
		 * If we have outstanding data (other than
		 * a window probe), this is a completely
		 * duplicate ack and the ack is the biggest we've seen.
		 *
		 * Need to accommodate a change in window on duplicate acks
		 * to allow operating systems that update window during
		 * recovery with SACK
		 */
		if (SEQ_LEQ(th->th_ack, tp->snd_una)) {
			/*
			 * Update snd_fack when new SACK blocks are received
			 * without advancing the ACK
			 */
			if (TCP_RACK_ENABLED(tp) && sack_bytes_acked > 0 &&
			    SEQ_LT(tp->snd_fack, highest_sacked_seq)) {
				tp->snd_fack = highest_sacked_seq;
			}

			/*
			 * Process AccECN feedback here for control packets
			 * that don't have s/acked bytes
			 */
			if (TCP_ACC_ECN_ON(tp) && (tp->ecn_flags & TE_SENDIPECT) &&
			    (sack_bytes_acked == 0)) {
				tp->total_ect_packets_acked += 1;

				bool newly_acked_time = false;
				if (acked == 0 && (to.to_flags & TOF_TS) != 0 && to.to_tsecr != 0 &&
				    TSTMP_GT(to.to_tsecr, tp->t_last_ack_tsecr)) {
					newly_acked_time = true;
				}
				if (newly_acked_time) {
					tcp_process_accecn(tp, &to, th, 1, ace);
				}
			}

			if (tlen == 0 && (tiwin == tp->snd_wnd ||
			    (to.to_nsacks > 0 && sack_bytes_acked > 0))) {
				uint32_t old_dupacks;
				/*
				 * If both ends send FIN at the same time,
				 * then the ack will be a duplicate ack
				 * but we have to process the FIN. Check
				 * for this condition and process the FIN
				 * instead of the dupack
				 */
				if ((thflags & TH_FIN) &&
				    !TCPS_HAVERCVDFIN(tp->t_state)) {
					break;
				}
process_dupack:
				old_dupacks = tp->t_dupacks;
#if MPTCP
				/*
				 * MPTCP options that are ignored must
				 * not be treated as duplicate ACKs.
				 */
				if (to.to_flags & TOF_MPTCP) {
					goto drop;
				}

				if ((isconnected) && (tp->t_mpflags & TMPF_JOINED_FLOW)) {
					break;
				}
#endif /* MPTCP */
				/*
				 * If a duplicate acknowledgement was seen
				 * after ECN, it indicates packet loss in
				 * addition to ECN. Reset INRECOVERY flag
				 * so that we can process partial acks
				 * correctly
				 */
				if (tp->ecn_flags & TE_INRECOVERY) {
					tp->ecn_flags &= ~TE_INRECOVERY;
				}

				tcpstat.tcps_rcvdupack++;
				if (SACK_ENABLED(tp)) {
					tp->t_dupacks += max(1, sack_bytes_acked / tp->t_maxseg);
				} else {
					++tp->t_dupacks;
				}

				tp->sackhint.sack_bytes_acked += sack_bytes_acked;

				if (sack_bytes_acked > 0 && TCP_ACC_ECN_ON(tp) &&
				    (tp->ecn_flags & TE_SENDIPECT) && tp->t_state == TCPS_ESTABLISHED) {
					uint32_t pkts_sacked = tcp_packets_this_ack(tp, sack_bytes_acked);
					tp->total_ect_packets_acked += pkts_sacked;
					tcp_process_accecn(tp, &to, th, pkts_sacked, ace);
				}
				/*
				 * Check if we need to reset the limit on
				 * early retransmit
				 */
				if (tp->t_early_rexmt_count > 0 &&
				    TSTMP_GEQ(tcp_now,
				    (tp->t_early_rexmt_win +
				    TCP_EARLY_REXMT_WIN))) {
					tp->t_early_rexmt_count = 0;
				}

				/*
				 * Is early retransmit needed? We check for
				 * this when the connection is waiting for
				 * duplicate acks to enter fast recovery.
				 */
				if (!IN_FASTRECOVERY(tp)) {
					tcp_early_rexmt_check(tp, th);
				}

				/*
				 * Detect loss based on RACK during dupACK processing to mark lost
				 * segments before tcp_output is called for retransmission
				 */
				if (TCP_RACK_ENABLED(tp) && tcp_rack_detect_loss_and_arm_timer(tp, tp->t_dupacks)) {
					rack_loss_detected = true;
				}
				/*
				 * Below are four different processing of (dup) ACKs,
				 * 1. Not a valid dup ACK
				 * 2. More than 3 dup ACKs but already in Fast Recovery
				 * 3. Entered Fast Recovery for the first time
				 * 4. Received less than 3 dup ACKs, evaluate if we can do Limited Transmit
				 */
				if (tp->t_timer[TCPT_REXMT] == 0 ||
				    (th->th_ack != tp->snd_una && sack_bytes_acked == 0)) {
					/*
					 * No outstanding data and ACK is not a duplicate as it is
					 * less than snd_una but not equal to it.
					 */
					tp->t_dupacks = 0;
					tp->t_rexmtthresh = tcprexmtthresh;
				} else if ((!TCP_RACK_ENABLED(tp) && tp->t_dupacks > tp->t_rexmtthresh && old_dupacks >= tp->t_rexmtthresh) ||
				    IN_FASTRECOVERY(tp)) {
					/*
					 * We are already in Fast Recovery and t_dupacks is greater than retransmit threshold.
					 * Increase the cwnd by 1MSS if allowed
					 */

					/*
					 * If this connection was seeing packet
					 * reordering, then recovery might be
					 * delayed to disambiguate between
					 * reordering and loss
					 */
					if (SACK_ENABLED(tp) && !IN_FASTRECOVERY(tp) &&
					    (tp->t_flagsext &
					    (TF_PKTS_REORDERED | TF_DELAY_RECOVERY)) ==
					    (TF_PKTS_REORDERED | TF_DELAY_RECOVERY)) {
						/*
						 * Since the SACK information is already
						 * updated, this ACK will be dropped
						 */
						break;
					}

					/*
					 * Dup acks mean that packets have left the
					 * network (they're now cached at the receiver)
					 * so bump cwnd by the amount in the receiver
					 * to keep a constant cwnd packets in the
					 * network.
					 */
					if (SACK_ENABLED(tp) && IN_FASTRECOVERY(tp)) {
						int awnd;

						/*
						 * Compute the amount of data in flight first.
						 * We can inject new data into the pipe iff
						 * we have less than snd_ssthres worth of data in
						 * flight.
						 */
						awnd = (tp->snd_nxt - tp->snd_fack) + tp->sackhint.sack_bytes_rexmit;
						if (awnd < tp->snd_ssthresh) {
							tp->snd_cwnd += tp->t_maxseg;
							if (tp->snd_cwnd > tp->snd_ssthresh) {
								tp->snd_cwnd = tp->snd_ssthresh;
							}
						}
					} else {
						tp->snd_cwnd += tp->t_maxseg;
					}

					/* Process any window updates */
					if (tiwin > tp->snd_wnd) {
						tcp_update_window(tp, thflags,
						    th, tiwin, tlen);
					}
					tcp_ccdbg_trace(tp, th,
					    TCP_CC_IN_FASTRECOVERY);

					(void) tcp_output(tp);

					goto drop;
				} else if (rack_loss_detected || (!TCP_RACK_ENABLED(tp) && tp->t_dupacks >= tp->t_rexmtthresh)) {
					/*
					 * Currently not in Fast Recovery and received 3 or more dupacks.
					 * Enter Fast Recovery, retransmit segment and set
					 * cwnd to sshthresh if SACK is enabled.
					 */
					tcp_seq onxt = tp->snd_nxt;

					/*
					 * If we're doing sack, check to
					 * see if we're already in sack
					 * recovery. If we're not doing sack,
					 * check to see if we're in newreno
					 * recovery.
					 */
					if (SACK_ENABLED(tp)) {
						if (IN_FASTRECOVERY(tp)) {
							tp->t_dupacks = 0;
							break;
						} else if (tp->t_flagsext & TF_DELAY_RECOVERY) {
							break;
						}
					} else {
						if (SEQ_LEQ(th->th_ack, tp->snd_recover)) {
							tp->t_dupacks = 0;
							break;
						}
					}
					if (tp->t_flags & TF_SENTFIN) {
						tp->snd_recover = tp->snd_max - 1;
					} else {
						tp->snd_recover = tp->snd_max;
					}
					tp->t_timer[TCPT_PTO] = 0;
					tp->t_rtttime = 0;

					/*
					 * If the connection has seen pkt
					 * reordering, delay recovery until
					 * it is clear that the packet
					 * was lost.
					 */
					if (SACK_ENABLED(tp) &&
					    (tp->t_flagsext &
					    (TF_PKTS_REORDERED | TF_DELAY_RECOVERY))
					    == TF_PKTS_REORDERED &&
					    !IN_FASTRECOVERY(tp) &&
					    tp->t_reorderwin > 0 &&
					    (tp->t_state == TCPS_ESTABLISHED ||
					    tp->t_state == TCPS_FIN_WAIT_1)) {
						tp->t_timer[TCPT_DELAYFR] =
						    OFFSET_FROM_START(tp,
						    tp->t_reorderwin);
						tp->t_flagsext |= TF_DELAY_RECOVERY;
						tcpstat.tcps_delay_recovery++;
						tcp_ccdbg_trace(tp, th,
						    TCP_CC_DELAY_FASTRECOVERY);
						break;
					}

					tcp_rexmt_save_state(tp);
					/*
					 * If the current tcp cc module has
					 * defined a hook for tasks to run
					 * before entering FR, call it
					 */
					if (CC_ALGO(tp)->pre_fr != NULL) {
						CC_ALGO(tp)->pre_fr(tp);
					}
					ENTER_FASTRECOVERY(tp);
					tp->t_timer[TCPT_REXMT] = 0;
					if (!TCP_ACC_ECN_ON(tp) && TCP_ECN_ENABLED(tp)) {
						tp->ecn_flags |= TE_SENDCWR;
					}

					if (SACK_ENABLED(tp)) {
						if (TCP_RACK_ENABLED(tp)) {
							tcpstat.tcps_rack_recovery_episode++;
							tp->t_rack_recovery_episode++;
						} else {
							tcpstat.tcps_sack_recovery_episode++;
							tp->t_sack_recovery_episode++;
						}

						tp->snd_cwnd = tp->snd_ssthresh;
						tp->t_flagsext &= ~TF_CWND_NONVALIDATED;

						/* Process any window updates */
						if (tiwin > tp->snd_wnd) {
							tcp_update_window(tp, thflags, th, tiwin, tlen);
						}

						tcp_ccdbg_trace(tp, th, TCP_CC_ENTER_FASTRECOVERY);
						(void) tcp_output(tp);
						goto drop;
					}
					tp->snd_nxt = th->th_ack;
					tp->snd_cwnd = tp->t_maxseg;

					/* cwnd is validated after pre_fr() */
					tp->t_flagsext &= ~TF_CWND_NONVALIDATED;

					/* Process any window updates */
					if (tiwin > tp->snd_wnd) {
						tcp_update_window(tp, thflags, th, tiwin, tlen);
					}

					(void) tcp_output(tp);
					if (tp->t_flagsext & TF_CWND_NONVALIDATED) {
						tcp_cc_adjust_nonvalidated_cwnd(tp);
					} else {
						tp->snd_cwnd = tp->snd_ssthresh + tp->t_maxseg * tp->t_dupacks;
					}
					if (SEQ_GT(onxt, tp->snd_nxt)) {
						tp->snd_nxt = onxt;
					}

					tcp_ccdbg_trace(tp, th, TCP_CC_ENTER_FASTRECOVERY);
					goto drop;
				} else if (ALLOW_LIMITED_TRANSMIT(tp) &&
				    (!(SACK_ENABLED(tp)) || sack_bytes_acked > 0) &&
				    (so->so_snd.sb_cc - (tp->snd_max - tp->snd_una)) > 0) {
					u_int32_t incr = (tp->t_maxseg * tp->t_dupacks);

					/* Use Limited Transmit algorithm on the first two
					 * duplicate acks when there is new data to transmit
					 */
					tp->snd_cwnd += incr;
					tcpstat.tcps_limited_txt++;
					(void) tcp_output(tp);

					tcp_ccdbg_trace(tp, th, TCP_CC_LIMITED_TRANSMIT);

					/* Reset snd_cwnd back to normal */
					tp->snd_cwnd -= incr;
				}
			}
			break;
		}
		/*
		 * If the congestion window was inflated to account
		 * for the other side's cached packets, retract it.
		 */
		if (IN_FASTRECOVERY(tp)) {
			if (SEQ_LT(th->th_ack, tp->snd_recover)) {
				/*
				 * If we received an ECE and entered
				 * recovery, the subsequent ACKs should
				 * not be treated as partial acks.
				 */
				if (tp->ecn_flags & TE_INRECOVERY) {
					goto process_ACK;
				}
				/* RACK doesn't require inflating cwnd */
				if (!TCP_RACK_ENABLED(tp)) {
					if (SACK_ENABLED(tp)) {
						tcp_sack_partialack(tp, th);
					} else {
						tcp_newreno_partial_ack(tp, th);
					}
					tcp_ccdbg_trace(tp, th, TCP_CC_PARTIAL_ACK);
				}
			} else {
				if (tcp_cubic_minor_fixes) {
					exiting_fr = 1;
				}
				EXIT_FASTRECOVERY(tp);
				if (CC_ALGO(tp)->post_fr != NULL) {
					CC_ALGO(tp)->post_fr(tp, th);
				}

				if (TCP_RACK_ENABLED(tp)) {
					tcp_rack_update_reordering_win_persist(tp);
				}

				tp->t_pipeack = 0;
				tcp_clear_pipeack_state(tp);
				tcp_ccdbg_trace(tp, th,
				    TCP_CC_EXIT_FASTRECOVERY);
			}
		} else if ((tp->t_flagsext &
		    (TF_PKTS_REORDERED | TF_DELAY_RECOVERY))
		    == (TF_PKTS_REORDERED | TF_DELAY_RECOVERY)) {
			/*
			 * If the ack acknowledges upto snd_recover or if
			 * it acknowledges all the snd holes, exit
			 * recovery and cancel the timer. Otherwise,
			 * this is a partial ack. Wait for recovery timer
			 * to enter recovery. The snd_holes have already
			 * been updated.
			 */
			if (SEQ_GEQ(th->th_ack, tp->snd_recover) ||
			    TAILQ_EMPTY(&tp->snd_holes)) {
				tp->t_timer[TCPT_DELAYFR] = 0;
				tp->t_flagsext &= ~TF_DELAY_RECOVERY;
				EXIT_FASTRECOVERY(tp);
				tcp_ccdbg_trace(tp, th,
				    TCP_CC_EXIT_FASTRECOVERY);
			}
		} else {
			/*
			 * We were not in fast recovery. Reset the
			 * duplicate ack counter.
			 */
			tp->t_dupacks = 0;
			tp->t_rexmtthresh = tcprexmtthresh;
		}

process_ACK:
		VERIFY(SEQ_GEQ(th->th_ack, tp->snd_una));
		acked = BYTES_ACKED(th, tp);
		tcpstat.tcps_rcvackpack++;
		tcpstat.tcps_rcvackbyte += acked;

		/*
		 * If the last packet was a retransmit, make sure
		 * it was not spurious.
		 *
		 * This will also take care of congestion window
		 * adjustment if a last packet was recovered due to a
		 * tail loss probe.
		 */
		tcp_bad_rexmt_check(tp, th, &to);

		/* Recalculate the RTT */
		tcp_compute_rtt(tp, &to, th);

		/*
		 * If all outstanding data is acked, stop retransmit
		 * timer and remember to restart (more output or persist).
		 * If there is more data to be acked, restart retransmit
		 * timer, using current (possibly backed-off) value.
		 */
		TCP_RESET_REXMT_STATE(tp);
		TCPT_RANGESET(tp->t_rxtcur, TCP_REXMTVAL(tp),
		    tp->t_rttmin, TCPTV_REXMTMAX,
		    TCP_ADD_REXMTSLOP(tp));
		if (th->th_ack == tp->snd_max) {
			tp->t_timer[TCPT_REXMT] = 0;
			tp->t_timer[TCPT_PTO] = 0;
			tp->t_timer[TCPT_REORDER] = 0;
			tcp_rack_reset_segs_retransmitted(tp);
			needoutput = 1;
		} else if (tp->t_timer[TCPT_PERSIST] == 0) {
			tp->t_timer[TCPT_REXMT] = OFFSET_FROM_START(tp,
			    tp->t_rxtcur);
		}

		if ((prev_t_state == TCPS_SYN_SENT ||
		    prev_t_state == TCPS_SYN_RECEIVED) &&
		    tp->t_state == TCPS_ESTABLISHED) {
			TCP_LOG_RTT_INFO(tp);
		}

		/*
		 * If no data (only SYN) was ACK'd, skip rest of ACK
		 * processing.
		 */
		if (acked == 0) {
			goto step6;
		}

		/*
		 * Process sent segments used for RACK as we need to update
		 * RACK state before loss detection. Update snd_fack only
		 * after ACK processing which performs reordering detection.
		 */
		if (TCP_RACK_ENABLED(tp)) {
			tcp_segs_doack(tp, th->th_ack, &to);
			if (SEQ_LT(tp->snd_fack, highest_sacked_seq)) {
				tp->snd_fack = highest_sacked_seq;
			}
			if (SEQ_LT(tp->snd_fack, th->th_ack)) {
				tp->snd_fack = th->th_ack;
			}
		}
		/*
		 * When outgoing data has been acked (except the SYN+data), we
		 * mark this connection as "sending good" for TFO.
		 */
		if ((tp->t_tfo_stats & TFO_S_SYN_DATA_SENT) &&
		    !(tp->t_tfo_flags & TFO_F_NO_SNDPROBING) &&
		    !(th->th_flags & TH_SYN)) {
			tp->t_tfo_flags |= TFO_F_NO_SNDPROBING;
		}

		if ((tp->ecn_flags & TE_SENDIPECT)) {
			/*
			 * draft-ietf-tcpm-accurate-ecn-28
			 * Accurate ECN feedback processing for data sender,
			 * Process peer's feedback in received TCP thflags and update s.cep
			 * Since SYN-ACK has a special encoding, exclude it from below.
			 * Only perform it before CC is called and snd_una is updated.
			 */
			if (TCP_ACC_ECN_ON(tp) && !(thflags & TH_SYN)) {
				/*
				 * For a server in SYN_RECEIVED state (that switched to
				 * ESTABLISHED in this ACK, exclude processing the last ACK
				 */
				if (th->th_ack == tp->iss + 1) {
					acked = 0;
				}
				uint32_t pkts_acked = tcp_packets_this_ack(tp, acked);
				tp->total_ect_packets_acked += pkts_acked;
				/*
				 * Calculate newly_acked_time used for AccECN feedback parsing
				 * for data sender if ACK acknowledges packets without data
				 * if reordering happens and certain packets have same TS.
				 * Right now, we consider that new time was ACKed if the TS
				 * was GT previous value, but we need to think about how to
				 * differentiate between reordering and wrapping when TS is same
				 * as previous value.
				 */
				bool newly_acked_time = false;
				if (acked == 0 && sack_bytes_acked == 0 &&
				    (to.to_flags & TOF_TS) != 0 && to.to_tsecr != 0 &&
				    (tp->t_last_ack_tsecr == 0 || TSTMP_GT(to.to_tsecr, tp->t_last_ack_tsecr))) {
					newly_acked_time = true;
				}
				/*
				 * Update s.cep if bytes have been newly S/ACKed
				 * otherwise, this ACK has already been superseded.
				 */
				if (acked > 0 || sack_bytes_acked > 0 || newly_acked_time) {
					tcp_process_accecn(tp, &to, th, pkts_acked, ace);
				}
			} else if (TCP_ECN_ENABLED(tp) && (thflags & TH_ECE)) {
				/*
				 * For classic ECN, congestion event is receiving TH_ECE.
				 * Reduce the congestion window if we haven't
				 * done so.
				 */
				if (!IN_FASTRECOVERY(tp)) {
					/*
					 * Although we enter Fast Recovery in the below function
					 * we exit it immediately below as th_ack >= snd_recover
					 */
					tcp_enter_fast_recovery(tp);
					tp->ecn_flags |= (TE_INRECOVERY | TE_SENDCWR);
					/*
					 * Also note that the connection received
					 * ECE atleast once. We increment
					 * t_ecn_capable_packets_marked when we first
					 * enter fast recovery.
					 */
					tp->ecn_flags |= TE_RECV_ECN_ECE;
					INP_INC_IFNET_STAT(inp, ecn_recv_ece);
					tcpstat.tcps_ecn_recv_ece++;
					tp->t_ecn_capable_packets_marked++;
					tcp_ccdbg_trace(tp, th, TCP_CC_ECN_RCVD);
				}
			}
		}

		/*
		 * When new data is acked, open the congestion window.
		 * The specifics of how this is achieved are up to the
		 * congestion control algorithm in use for this connection.
		 *
		 * The calculations in this function assume that snd_una is
		 * not updated yet.
		 */
		if (!IN_FASTRECOVERY(tp) && !exiting_fr) {
			if (CC_ALGO(tp)->ack_rcvd != NULL) {
				CC_ALGO(tp)->ack_rcvd(tp, th);
			}
			tcp_ccdbg_trace(tp, th, TCP_CC_ACK_RCVD);
		}
		if (acked > so->so_snd.sb_cc) {
			tp->snd_wnd -= so->so_snd.sb_cc;
			sbdrop(&so->so_snd, (int)so->so_snd.sb_cc);
			ourfinisacked = 1;
		} else {
			sbdrop(&so->so_snd, acked);
			tcp_sbsnd_trim(&so->so_snd);
			tp->snd_wnd -= acked;
			ourfinisacked = 0;
		}
		/* detect una wraparound */
		if (!IN_FASTRECOVERY(tp) &&
		    SEQ_GT(tp->snd_una, tp->snd_recover) &&
		    SEQ_LEQ(th->th_ack, tp->snd_recover)) {
			tp->snd_recover = th->th_ack - 1;
		}

		if (IN_FASTRECOVERY(tp) &&
		    SEQ_GEQ(th->th_ack, tp->snd_recover)) {
			EXIT_FASTRECOVERY(tp);
			if (TCP_RACK_ENABLED(tp)) {
				tcp_rack_update_reordering_win_persist(tp);
			}
		}

		tcp_update_snd_una(tp, th->th_ack);

		if (SACK_ENABLED(tp)) {
			if (SEQ_GT(tp->snd_una, tp->snd_recover)) {
				tp->snd_recover = tp->snd_una;
			}
		}
		if (SEQ_LT(tp->snd_nxt, tp->snd_una)) {
			tp->snd_nxt = tp->snd_una;
		}

		/*
		 * Detect loss based on RACK during ACK processing to mark lost
		 * segments and call tcp_output. Rest of the ACK processing can
		 * continue after that.
		 */
		if (TCP_RACK_ENABLED(tp) && tcp_rack_detect_loss_and_arm_timer(tp, 0)) {
			if (!IN_FASTRECOVERY(tp)) {
				tcp_enter_fast_recovery(tp);
				tcpstat.tcps_rack_recovery_episode++;
				tp->t_rack_recovery_episode++;
			}
			tcp_output(tp);
		}

		if (!SLIST_EMPTY(&tp->t_rxt_segments) &&
		    !TCP_DSACK_SEQ_IN_WINDOW(tp, tp->t_dsack_lastuna,
		    tp->snd_una)) {
			tcp_rxtseg_clean(tp);
		}
		if ((tp->t_flagsext & TF_MEASURESNDBW) != 0 &&
		    tp->t_bwmeas != NULL) {
			tcp_bwmeas_check(tp);
		}

		write_wakeup = 1;

		if (!SLIST_EMPTY(&tp->t_notify_ack)) {
			tcp_notify_acknowledgement(tp, so);
		}

		switch (tp->t_state) {
		/*
		 * In FIN_WAIT_1 STATE in addition to the processing
		 * for the ESTABLISHED state if our FIN is now acknowledged
		 * then enter FIN_WAIT_2.
		 */
		case TCPS_FIN_WAIT_1:
			if (ourfinisacked) {
				/*
				 * If we can't receive any more
				 * data, then closing user can proceed.
				 * Starting the TCPT_2MSL timer is contrary to the
				 * specification, but if we don't get a FIN
				 * we'll hang forever.
				 */
				DTRACE_TCP4(state__change, void, NULL,
				    struct inpcb *, inp,
				    struct tcpcb *, tp,
				    int32_t, TCPS_FIN_WAIT_2);
				TCP_LOG_STATE(tp, TCPS_FIN_WAIT_2);
				tp->t_state = TCPS_FIN_WAIT_2;
				if (so->so_state & SS_CANTRCVMORE) {
					isconnected = FALSE;
					isdisconnected = TRUE;
					tcp_set_finwait_timeout(tp);
				}
				/*
				 * fall through and make sure we also recognize
				 * data ACKed with the FIN
				 */
			}
			break;

		/*
		 * In CLOSING STATE in addition to the processing for
		 * the ESTABLISHED state if the ACK acknowledges our FIN
		 * then enter the TIME-WAIT state, otherwise ignore
		 * the segment.
		 */
		case TCPS_CLOSING:
			if (ourfinisacked) {
				DTRACE_TCP4(state__change, void, NULL,
				    struct inpcb *, inp,
				    struct tcpcb *, tp,
				    int32_t, TCPS_TIME_WAIT);
				TCP_LOG_STATE(tp, TCPS_TIME_WAIT);
				tp->t_state = TCPS_TIME_WAIT;
				tcp_canceltimers(tp);
				if (tp->t_flagsext & TF_NOTIMEWAIT) {
					tp->t_flags |= TF_CLOSING;
				} else {
					add_to_time_wait(tp, 2 * tcp_msl);
				}
				isconnected = FALSE;
				isdisconnected = TRUE;
			}
			break;

		/*
		 * In LAST_ACK, we may still be waiting for data to drain
		 * and/or to be acked, as well as for the ack of our FIN.
		 * If our FIN is now acknowledged, delete the TCB,
		 * enter the closed state and return.
		 */
		case TCPS_LAST_ACK:
			if (ourfinisacked) {
				tp = tcp_close(tp);
				goto drop;
			}
			break;

		/*
		 * In TIME_WAIT state the only thing that should arrive
		 * is a retransmission of the remote FIN.  Acknowledge
		 * it and restart the finack timer.
		 */
		case TCPS_TIME_WAIT:
			add_to_time_wait(tp, 2 * tcp_msl);
			goto dropafterack;
		}

		/*
		 * If there is a SACK option on the ACK and we
		 * haven't seen any duplicate acks before, count
		 * it as a duplicate ack even if the cumulative
		 * ack is advanced. If the receiver delayed an
		 * ack and detected loss afterwards, then the ack
		 * will advance cumulative ack and will also have
		 * a SACK option. So counting it as one duplicate
		 * ack is ok.
		 */
		if (tp->t_state == TCPS_ESTABLISHED &&
		    SACK_ENABLED(tp) && sack_bytes_acked > 0 &&
		    to.to_nsacks > 0 && tp->t_dupacks == 0 &&
		    SEQ_LEQ(th->th_ack, tp->snd_una) && tlen == 0 &&
		    !(tp->t_flagsext & TF_PKTS_REORDERED)) {
			tcpstat.tcps_sack_ackadv++;
			goto process_dupack;
		}
	}

step6:
	/*
	 * Update window information.
	 */
	if (tcp_update_window(tp, thflags, th, tiwin, tlen)) {
		needoutput = 1;
	}

	/*
	 * Process segments with URG.
	 */
	if ((thflags & TH_URG) && th->th_urp &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		/*
		 * This is a kludge, but if we receive and accept
		 * random urgent pointers, we'll crash in
		 * soreceive.  It's hard to imagine someone
		 * actually wanting to send this much urgent data.
		 */
		if (th->th_urp + so->so_rcv.sb_cc > sb_max) {
			th->th_urp = 0;                 /* XXX */
			thflags &= ~TH_URG;             /* XXX */
			goto dodata;                    /* XXX */
		}
		/*
		 * If this segment advances the known urgent pointer,
		 * then mark the data stream.  This should not happen
		 * in CLOSE_WAIT, CLOSING, LAST_ACK or TIME_WAIT STATES since
		 * a FIN has been received from the remote side.
		 * In these states we ignore the URG.
		 *
		 * According to RFC961 (Assigned Protocols),
		 * the urgent pointer points to the last octet
		 * of urgent data.  We continue, however,
		 * to consider it to indicate the first octet
		 * of data past the urgent section as the original
		 * spec states (in one of two places).
		 */
		if (SEQ_GT(th->th_seq + th->th_urp, tp->rcv_up)) {
			tp->rcv_up = th->th_seq + th->th_urp;
			so->so_oobmark = so->so_rcv.sb_cc +
			    (tp->rcv_up - tp->rcv_nxt) - 1;
			if (so->so_oobmark == 0) {
				so->so_state |= SS_RCVATMARK;
			}
			sohasoutofband(so);
			tp->t_oobflags &= ~(TCPOOB_HAVEDATA | TCPOOB_HADDATA);
		}
		/*
		 * Remove out of band data so doesn't get presented to user.
		 * This can happen independent of advancing the URG pointer,
		 * but if two URG's are pending at once, some out-of-band
		 * data may creep in... ick.
		 */
		if (th->th_urp <= (u_int32_t)tlen
#if SO_OOBINLINE
		    && (so->so_options & SO_OOBINLINE) == 0
#endif
		    ) {
			tcp_pulloutofband(so, th, m,
			    drop_hdrlen);       /* hdr drop is delayed */
		}
	} else {
		/*
		 * If no out of band data is expected,
		 * pull receive urgent pointer along
		 * with the receive window.
		 */
		if (SEQ_GT(tp->rcv_nxt, tp->rcv_up)) {
			tp->rcv_up = tp->rcv_nxt;
		}
	}
dodata:

	/* Set socket's connect or disconnect state correcly before doing data.
	 * The following might unlock the socket if there is an upcall or a socket
	 * filter.
	 */
	if (isconnected) {
		soisconnected(so);
	} else if (isdisconnected) {
		soisdisconnected(so);
	}

	/* Let's check the state of pcb just to make sure that it did not get closed
	 * when we unlocked above
	 */
	if (inp->inp_state == INPCB_STATE_DEAD) {
		/* Just drop the packet that we are processing and return */
		TCP_LOG_DROP_PCB(TCP_LOG_HDR, th, tp, false, "INPCB_STATE_DEAD");
		goto drop;
	}

	/*
	 * Process the segment text, merging it into the TCP sequencing queue,
	 * and arranging for acknowledgment of receipt if necessary.
	 * This process logically involves adjusting tp->rcv_wnd as data
	 * is presented to the user (this happens in tcp_usrreq.c,
	 * case PRU_RCVD).  If a FIN has already been received on this
	 * connection then we just ignore the text.
	 *
	 * If we are in SYN-received state and got a valid TFO cookie, we want
	 * to process the data.
	 */
	if ((tlen || (thflags & TH_FIN)) &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0 &&
	    (TCPS_HAVEESTABLISHED(tp->t_state) ||
	    (tp->t_state == TCPS_SYN_RECEIVED &&
	    (tp->t_tfo_flags & TFO_F_COOKIE_VALID)))) {
		tcp_seq save_start = th->th_seq;
		tcp_seq save_end = th->th_seq + tlen;
		m_adj(m, drop_hdrlen);  /* delayed header drop */
		/*
		 * Insert segment which includes th into TCP reassembly queue
		 * with control block tp.  Set thflags to whether reassembly now
		 * includes a segment with FIN.  This handles the common case
		 * inline (segment is the next to be received on an established
		 * connection, and the queue is empty), avoiding linkage into
		 * and removal from the queue and repetition of various
		 * conversions.
		 * Set DELACK for segments received in order, but ack
		 * immediately when segments are out of order (so
		 * fast retransmit can work).
		 */
		if (th->th_seq == tp->rcv_nxt && LIST_EMPTY(&tp->t_segq)) {
			TCP_INC_VAR(tp->t_unacksegs, segment_count);

			/* Calculate the RTT on the receiver */
			tcp_compute_rcv_rtt(tp, &to, th);

			if (DELAY_ACK(tp, th) &&
			    ((tp->t_flags & TF_ACKNOW) == 0)) {
				if ((tp->t_flags & TF_DELACK) == 0) {
					tp->t_flags |= TF_DELACK;
					tp->t_timer[TCPT_DELACK] =
					    OFFSET_FROM_START(tp, tcp_delack);
				}
			} else {
				tp->t_flags |= TF_ACKNOW;
			}
			tp->rcv_nxt += tlen;
			/* Update highest received sequence and its timestamp */
			if (SEQ_LT(tp->rcv_high, tp->rcv_nxt)) {
				tp->rcv_high = tp->rcv_nxt;
				if (to.to_flags & TOF_TS) {
					tp->tsv_high = to.to_tsval;
				}
			}

			thflags = th->th_flags & TH_FIN;
			TCP_INC_VAR(tcpstat.tcps_rcvpack, segment_count);
			tcpstat.tcps_rcvbyte += tlen;
			if (nstat_collect) {
				INP_ADD_STAT(inp, ifnet_count_type,
				    rxpackets, 1);
				INP_ADD_STAT(inp, ifnet_count_type,
				    rxbytes, tlen);
				inp_set_activity_bitmap(inp);
			}
			tcp_sbrcv_grow(tp, &so->so_rcv, &to, tlen);
			if (TCP_USE_RLEDBAT(tp, so) &&
			    tcp_cc_rledbat.data_rcvd != NULL) {
				tcp_cc_rledbat.data_rcvd(tp, th, &to, tlen);
			}

			so_recv_data_stat(so, m, drop_hdrlen);

			if (isipv6) {
				memcpy(&saved_hdr, ip6, sizeof(struct ip6_hdr));
				ip6 = (struct ip6_hdr *)&saved_hdr[0];
			} else {
				memcpy(&saved_hdr, ip, ip->ip_hl << 2);
				ip = (struct ip *)&saved_hdr[0];
			}
			memcpy(&saved_tcphdr, th, sizeof(struct tcphdr));

			if (th->th_flags & TH_PUSH) {
				tp->t_flagsext |= TF_LAST_IS_PSH;
			} else {
				tp->t_flagsext &= ~TF_LAST_IS_PSH;
			}

			if (sbappendstream_rcvdemux(so, m)) {
				read_wakeup = 1;
			}
			th = &saved_tcphdr;
		} else {
			if (isipv6) {
				memcpy(&saved_hdr, ip6, sizeof(struct ip6_hdr));
				ip6 = (struct ip6_hdr *)&saved_hdr[0];
			} else {
				memcpy(&saved_hdr, ip, ip->ip_hl << 2);
				ip = (struct ip *)&saved_hdr[0];
			}

			/* Update highest received sequence and its timestamp */
			if (SEQ_LT(tp->rcv_high, th->th_seq + tlen)) {
				tp->rcv_high = th->th_seq + tlen;
				if (to.to_flags & TOF_TS) {
					tp->tsv_high = to.to_tsval;
				}
			}

			/*
			 * Calculate the RTT on the receiver,
			 * even if OOO segment is received.
			 */
			tcp_compute_rcv_rtt(tp, &to, th);

			if (tcp_autotune_reorder) {
				tcp_sbrcv_grow(tp, &so->so_rcv, &to, tlen);
			}
			if (TCP_USE_RLEDBAT(tp, so) &&
			    tcp_cc_rledbat.data_rcvd != NULL) {
				tcp_cc_rledbat.data_rcvd(tp, th, &to, tlen);
			}

			memcpy(&saved_tcphdr, th, sizeof(struct tcphdr));
			thflags = tcp_reass(tp, th, &tlen, m, ifp, &read_wakeup);
			th = &saved_tcphdr;
			tp->t_flags |= TF_ACKNOW;
		}

		if ((tlen > 0 || (th->th_flags & TH_FIN)) && SACK_ENABLED(tp)) {
			if (th->th_flags & TH_FIN) {
				save_end++;
			}
			tcp_update_sack_list(tp, save_start, save_end);
		}

		tcp_adaptive_rwtimo_check(tp, tlen);

		if (tlen > 0) {
			tcp_tfo_rcv_data(tp);
		}

		if (tp->t_flags & TF_DELACK) {
			if (isipv6) {
				KERNEL_DEBUG(DBG_LAYER_END, ((th->th_dport << 16) | th->th_sport),
				    (((ip6->ip6_src.s6_addr16[0]) << 16) | (ip6->ip6_dst.s6_addr16[0])),
				    th->th_seq, th->th_ack, th->th_win);
			} else {
				KERNEL_DEBUG(DBG_LAYER_END, ((th->th_dport << 16) | th->th_sport),
				    (((ip->ip_src.s_addr & 0xffff) << 16) | (ip->ip_dst.s_addr & 0xffff)),
				    th->th_seq, th->th_ack, th->th_win);
			}
		}
	} else {
		if ((so->so_flags & SOF_MP_SUBFLOW) && tlen == 0 &&
		    (m->m_pkthdr.pkt_flags & PKTF_MPTCP_DFIN) &&
		    (m->m_pkthdr.pkt_flags & PKTF_MPTCP)) {
			m_adj(m, drop_hdrlen);  /* delayed header drop */
			/*
			 * 0-length DATA_FIN. The rlen is actually 0. We special-case the
			 * byte consumed by the dfin in mptcp_input and mptcp_reass_present
			 */
			m->m_pkthdr.mp_rlen = 0;
			mptcp_input(tptomptp(tp)->mpt_mpte, m);
			tp->t_flags |= TF_ACKNOW;
		} else {
			m_freem(m);
		}
		thflags &= ~TH_FIN;
	}
	/*
	 * We increment t_unacksegs_ce for both data segments and pure ACKs
	 * No need to increment if a FIN has already been received.
	 */
	if (TCP_ACC_ECN_ON(tp) && TCPS_HAVEESTABLISHED(tp->t_state) &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		if (ip_ecn == IPTOS_ECN_CE) {
			TCP_INC_VAR(tp->t_unacksegs_ce, segment_count);
		}
		/*
		 * Send an ACK immediately if there is a change in IP ECN
		 * from non-CE to CE.
		 * If new data is delivered, then ACK for every 2 CE marks,
		 * otherwise ACK for every 3 CE marks
		 */
		if ((ip_ecn == IPTOS_ECN_CE && ip_ecn != tp->t_prev_ip_ecn) ||
		    (tp->t_unacksegs_ce >= 2 && tp->last_ack_sent != tp->rcv_nxt) ||
		    tp->t_unacksegs_ce >= 3) {
			tp->t_flags |= TF_ACKNOW;
		}
		tp->t_prev_ip_ecn = ip_ecn;
	}
	/*
	 * If FIN is received ACK the FIN and let the user know
	 * that the connection is closing.
	 */
	if (thflags & TH_FIN) {
		if (TCPS_HAVERCVDFIN(tp->t_state) == 0) {
			socantrcvmore(so);
			/*
			 * If connection is half-synchronized
			 * (ie NEEDSYN flag on) then delay ACK,
			 * so it may be piggybacked when SYN is sent.
			 * Otherwise, since we received a FIN then no
			 * more input can be expected, send ACK now.
			 */
			TCP_INC_VAR(tp->t_unacksegs, segment_count);
			tp->t_flags |= TF_ACKNOW;
			tp->rcv_nxt++;
		}
		switch (tp->t_state) {
		/*
		 * In SYN_RECEIVED and ESTABLISHED STATES
		 * enter the CLOSE_WAIT state.
		 */
		case TCPS_SYN_RECEIVED:
			tp->t_starttime = tcp_now;
			OS_FALLTHROUGH;
		case TCPS_ESTABLISHED:
			DTRACE_TCP4(state__change, void, NULL, struct inpcb *, inp,
			    struct tcpcb *, tp, int32_t, TCPS_CLOSE_WAIT);
			TCP_LOG_STATE(tp, TCPS_CLOSE_WAIT);
			tp->t_state = TCPS_CLOSE_WAIT;
			break;

		/*
		 * If still in FIN_WAIT_1 STATE FIN has not been acked so
		 * enter the CLOSING state.
		 */
		case TCPS_FIN_WAIT_1:
			DTRACE_TCP4(state__change, void, NULL, struct inpcb *, inp,
			    struct tcpcb *, tp, int32_t, TCPS_CLOSING);
			TCP_LOG_STATE(tp, TCPS_CLOSING);
			tp->t_state = TCPS_CLOSING;
			break;

		/*
		 * In FIN_WAIT_2 state enter the TIME_WAIT state,
		 * starting the time-wait timer, turning off the other
		 * standard timers.
		 */
		case TCPS_FIN_WAIT_2:
			DTRACE_TCP4(state__change, void, NULL,
			    struct inpcb *, inp,
			    struct tcpcb *, tp,
			    int32_t, TCPS_TIME_WAIT);
			TCP_LOG_STATE(tp, TCPS_TIME_WAIT);
			tp->t_state = TCPS_TIME_WAIT;
			tcp_canceltimers(tp);
			tp->t_flags |= TF_ACKNOW;
			if (tp->t_flagsext & TF_NOTIMEWAIT) {
				tp->t_flags |= TF_CLOSING;
			} else {
				add_to_time_wait(tp, 2 * tcp_msl);
			}
			soisdisconnected(so);
			break;

		/*
		 * In TIME_WAIT state restart the 2 MSL time_wait timer.
		 */
		case TCPS_TIME_WAIT:
			add_to_time_wait(tp, 2 * tcp_msl);
			break;
		}
	}
	if (read_wakeup) {
		mptcp_handle_input(so);
	}

	/*
	 * Return any desired output.
	 */
	if (needoutput || (tp->t_flags & TF_ACKNOW)) {
		(void) tcp_output(tp);
	}

	tcp_check_timer_state(tp);

	tcp_handle_wakeup(so, read_wakeup, write_wakeup);

	socket_unlock(so, 1);
	KERNEL_DEBUG(DBG_FNC_TCP_INPUT | DBG_FUNC_END, 0, 0, 0, 0, 0);
	return;

dropafterack:
	/*
	 * Generate an ACK dropping incoming segment if it occupies
	 * sequence space, where the ACK reflects our state.
	 *
	 * We can now skip the test for the RST flag since all
	 * paths to this code happen after packets containing
	 * RST have been dropped.
	 *
	 * In the SYN-RECEIVED state, don't send an ACK unless the
	 * segment we received passes the SYN-RECEIVED ACK test.
	 * If it fails send a RST.  This breaks the loop in the
	 * "LAND" DoS attack, and also prevents an ACK storm
	 * between two listening ports that have been sent forged
	 * SYN segments, each with the source address of the other.
	 */
	if (tp->t_state == TCPS_SYN_RECEIVED && (thflags & TH_ACK) &&
	    (SEQ_GT(tp->snd_una, th->th_ack) ||
	    SEQ_GT(th->th_ack, tp->snd_max))) {
		IF_TCP_STATINC(ifp, dospacket);
		goto dropwithreset;
	}
	m_freem(m);
	tp->t_flags |= TF_ACKNOW;

	(void) tcp_output(tp);

	tcp_handle_wakeup(so, read_wakeup, write_wakeup);

	/* Don't need to check timer state as we should have done it during tcp_output */
	socket_unlock(so, 1);
	KERNEL_DEBUG(DBG_FNC_TCP_INPUT | DBG_FUNC_END, 0, 0, 0, 0, 0);
	return;
dropwithresetnosock:
	nosock = 1;
dropwithreset:
	/*
	 * Generate a RST, dropping incoming segment.
	 * Make ACK acceptable to originator of segment.
	 * Don't bother to respond if destination was broadcast/multicast.
	 */
	if ((thflags & TH_RST) || m->m_flags & (M_BCAST | M_MCAST)) {
		goto drop;
	}
	if (isipv6) {
		if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
		    IN6_IS_ADDR_MULTICAST(&ip6->ip6_src)) {
			goto drop;
		}
	} else if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) ||
	    IN_MULTICAST(ntohl(ip->ip_src.s_addr)) ||
	    ip->ip_src.s_addr == htonl(INADDR_BROADCAST) ||
	    in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif)) {
		goto drop;
	}
	/* IPv6 anycast check is done at tcp6_input() */

	bzero(&tra, sizeof(tra));
	tra.ifscope = ifscope;
	tra.awdl_unrestricted = 1;
	tra.intcoproc_allowed = 1;
	tra.management_allowed = 1;
	if (thflags & TH_ACK) {
		/* mtod() below is safe as long as hdr dropping is delayed */
		tcp_respond(tp, mtod(m, void *), th, m, (tcp_seq)0, th->th_ack,
		    TH_RST, &tra);
	} else {
		if (thflags & TH_SYN) {
			tlen++;
		}
		/* mtod() below is safe as long as hdr dropping is delayed */
		tcp_respond(tp, mtod(m, void *), th, m, th->th_seq + tlen,
		    (tcp_seq)0, TH_RST | TH_ACK, &tra);
	}
	/* destroy temporarily created socket */
	if (dropsocket) {
		(void) soabort(so);
		socket_unlock(so, 1);
	} else if ((inp != NULL) && (nosock == 0)) {
		tcp_handle_wakeup(so, read_wakeup, write_wakeup);

		socket_unlock(so, 1);
	}
	KERNEL_DEBUG(DBG_FNC_TCP_INPUT | DBG_FUNC_END, 0, 0, 0, 0, 0);
	return;
dropnosock:
	nosock = 1;
drop:
	/*
	 * Drop space held by incoming segment and return.
	 */
	if (isipv6 == 0) {
		if (ip == NULL) {
			ip = mtod(m, struct ip *);
		}
		/* add back the header length */
		ip->ip_len += (ip->ip_hl << 2);
		HTONS(ip->ip_len);
		HTONS(ip->ip_off);

		th = (struct tcphdr *)(void *)((caddr_t)ip + off0);
	} else if (ip6 == NULL) {
		ip6 = mtod(m, struct ip6_hdr *);

		th = (struct tcphdr *)(void *)((caddr_t)ip6 + off0);
	}
	if (is_th_swapped) {
		HTONL(th->th_seq);
		HTONL(th->th_ack);
		HTONS(th->th_win);
		HTONS(th->th_urp);
	}
	m_drop(m, DROPTAP_FLAG_DIR_IN | DROPTAP_FLAG_L2_MISSING, drop_reason, NULL, 0);
	/* destroy temporarily created socket */
	if (dropsocket) {
		(void) soabort(so);
		socket_unlock(so, 1);
	} else if (nosock == 0) {
		tcp_handle_wakeup(so, read_wakeup, write_wakeup);

		socket_unlock(so, 1);
	}
	KERNEL_DEBUG(DBG_FNC_TCP_INPUT | DBG_FUNC_END, 0, 0, 0, 0, 0);
	return;
}

/*
 * Parse TCP options and place in tcpopt.
 */
static void
tcp_dooptions(struct tcpcb *tp, u_char *cp, int cnt, struct tcphdr *th,
    struct tcpopt *to)
{
	u_short mss = 0;
	uint8_t opt, optlen;

	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == TCPOPT_EOL) {
			break;
		}
		if (opt == TCPOPT_NOP) {
			optlen = 1;
		} else {
			if (cnt < 2) {
				break;
			}
			optlen = cp[1];
			if (optlen < 2 || optlen > cnt) {
				break;
			}
		}
		switch (opt) {
		default:
			continue;

		case TCPOPT_MAXSEG:
			if (optlen != TCPOLEN_MAXSEG) {
				continue;
			}
			if (!(th->th_flags & TH_SYN)) {
				continue;
			}
			bcopy((char *) cp + 2, (char *) &mss, sizeof(mss));
			NTOHS(mss);
			to->to_mss = mss;
			to->to_flags |= TOF_MSS;
			break;

		case TCPOPT_WINDOW:
			if (optlen != TCPOLEN_WINDOW) {
				continue;
			}
			if (!(th->th_flags & TH_SYN)) {
				continue;
			}
			to->to_flags |= TOF_SCALE;
			to->to_requested_s_scale = MIN(cp[2], TCP_MAX_WINSHIFT);
			break;

		case TCPOPT_TIMESTAMP:
			if (optlen != TCPOLEN_TIMESTAMP) {
				continue;
			}
			to->to_flags |= TOF_TS;
			bcopy((char *)cp + 2,
			    (char *)&to->to_tsval, sizeof(to->to_tsval));
			NTOHL(to->to_tsval);
			bcopy((char *)cp + 6,
			    (char *)&to->to_tsecr, sizeof(to->to_tsecr));
			NTOHL(to->to_tsecr);
			to->to_tsecr -= tp->t_ts_offset;
			/* Re-enable sending Timestamps if we received them */
			if (!(tp->t_flags & TF_REQ_TSTMP) && tcp_do_timestamps) {
				tp->t_flags |= TF_REQ_TSTMP;
			}
			break;
		case TCPOPT_SACK_PERMITTED:
			if (optlen != TCPOLEN_SACK_PERMITTED) {
				continue;
			}
			if (th->th_flags & TH_SYN) {
				to->to_flags |= TOF_SACK;
			}
			break;
		case TCPOPT_SACK:
			if (optlen <= 2 || (optlen - 2) % TCPOLEN_SACK != 0) {
				continue;
			}
			to->to_nsacks = (optlen - 2) / TCPOLEN_SACK;
			to->to_sacks = cp + 2;
			tcpstat.tcps_sack_rcv_blocks++;

			break;
		case TCPOPT_FASTOPEN:
			if (optlen == TCPOLEN_FASTOPEN_REQ) {
				if (tp->t_state != TCPS_LISTEN) {
					continue;
				}

				to->to_flags |= TOF_TFOREQ;
			} else {
				if (optlen < TCPOLEN_FASTOPEN_REQ ||
				    (optlen - TCPOLEN_FASTOPEN_REQ) > TFO_COOKIE_LEN_MAX ||
				    (optlen - TCPOLEN_FASTOPEN_REQ) < TFO_COOKIE_LEN_MIN) {
					continue;
				}
				if (tp->t_state != TCPS_LISTEN &&
				    tp->t_state != TCPS_SYN_SENT) {
					continue;
				}

				to->to_flags |= TOF_TFO;
				to->to_tfo = cp + 1;
			}

			break;
		case TCPOPT_ACCECN0:
		case TCPOPT_ACCECN1:
			if (optlen < (TCPOLEN_ACCECN_EMPTY + 1 * TCPOLEN_ACCECN_COUNTER) ||
			    (optlen - 2) % TCPOLEN_ACCECN_COUNTER != 0) {
				continue;
			}
			to->to_num_accecn = (optlen - 2) / TCPOLEN_ACCECN_COUNTER;
			to->to_accecn = cp + 2;
			if (opt == TCPOPT_ACCECN0) {
				to->to_accecn_order = 0;
			} else if (opt == TCPOPT_ACCECN1) {
				to->to_accecn_order = 1;
			}
			break;

#if MPTCP
		case TCPOPT_MULTIPATH:
			tcp_do_mptcp_options(tp, cp, th, to, optlen);
			break;
#endif /* MPTCP */
		}
	}
}

static void
tcp_finalize_options(struct tcpcb *tp, struct tcpopt *to, unsigned int ifscope)
{
	if (to->to_flags & TOF_TS) {
		tp->t_flags |= TF_RCVD_TSTMP;
		tp->ts_recent = to->to_tsval;
		tp->ts_recent_age = tcp_now;
	}
	if (to->to_flags & TOF_MSS) {
		tcp_mss(tp, to->to_mss, ifscope);
	}
	if (SACK_ENABLED(tp)) {
		if (!(to->to_flags & TOF_SACK)) {
			tp->t_flagsext &= ~(TF_SACK_ENABLE);
		} else {
			tp->t_flags |= TF_SACK_PERMIT;
		}
	}
	if (to->to_flags & TOF_SCALE) {
		tp->t_flags |= TF_RCVD_SCALE;
		tp->requested_s_scale = to->to_requested_s_scale;

		/* Re-enable window scaling, if the option is received */
		if (tp->request_r_scale > 0) {
			tp->t_flags |= TF_REQ_SCALE;
		}
	}
}

/*
 * Pull out of band byte out of a segment so
 * it doesn't appear in the user's data queue.
 * It is still reflected in the segment length for
 * sequencing purposes.
 *
 * @param off delayed to be droped hdrlen
 */
static void
tcp_pulloutofband(struct socket *so, struct tcphdr *th, struct mbuf *m, int off)
{
	int cnt = off + th->th_urp - 1;

	while (cnt >= 0) {
		if (m->m_len > cnt) {
			char *cp = mtod(m, caddr_t) + cnt;
			struct tcpcb *tp = sototcpcb(so);

			tp->t_iobc = *cp;
			tp->t_oobflags |= TCPOOB_HAVEDATA;
			bcopy(cp + 1, cp, (unsigned)(m->m_len - cnt - 1));
			m->m_len--;
			if (m->m_flags & M_PKTHDR) {
				m->m_pkthdr.len--;
			}
			return;
		}
		cnt -= m->m_len;
		m = m->m_next;
		if (m == 0) {
			break;
		}
	}
	panic("tcp_pulloutofband");
}

uint32_t
get_base_rtt(struct tcpcb *tp)
{
	struct rtentry *rt = tp->t_inpcb->inp_route.ro_rt;
	return (rt == NULL) ? 0 : rt->rtt_min;
}

static void
update_curr_rtt(struct tcpcb * tp, uint32_t rtt)
{
	tp->curr_rtt_index = (tp->curr_rtt_index + 1) % NCURR_RTT_HIST;
	tp->curr_rtt_hist[tp->curr_rtt_index] = rtt;

	/* forget the old value and update minimum */
	tp->curr_rtt_min = 0;
	for (int i = 0; i < NCURR_RTT_HIST; ++i) {
		if (tp->curr_rtt_hist[i] != 0 && (tp->curr_rtt_min == 0 ||
		    tp->curr_rtt_hist[i] < tp->curr_rtt_min)) {
			tp->curr_rtt_min = tp->curr_rtt_hist[i];
		}
	}
}

/* Each value of RTT base represents the minimum RTT seen in a minute.
 * We keep upto N_RTT_BASE minutes worth of history.
 */
void
update_base_rtt(struct tcpcb *tp, uint32_t rtt)
{
	u_int32_t base_rtt, i;
	struct rtentry *rt;

	if ((rt = tp->t_inpcb->inp_route.ro_rt) == NULL) {
		return;
	}
	if (rt->rtt_expire_ts == 0) {
		RT_LOCK_SPIN(rt);
		if (rt->rtt_expire_ts != 0) {
			RT_UNLOCK(rt);
			goto update;
		}
		rt->rtt_expire_ts = tcp_now;
		rt->rtt_index = 0;
		rt->rtt_hist[0] = rtt;
		rt->rtt_min = rtt;
		RT_UNLOCK(rt);

		tp->curr_rtt_index = 0;
		tp->curr_rtt_hist[0] = rtt;
		tp->curr_rtt_min = rtt;
		return;
	}
update:
#if TRAFFIC_MGT
	/*
	 * If the recv side is being throttled, check if the
	 * current RTT is closer to the base RTT seen in
	 * first (recent) two slots. If so, unthrottle the stream.
	 */
	if ((tp->t_flagsext & TF_RECV_THROTTLE) &&
	    (int)(tcp_now - tp->t_recv_throttle_ts) >= TCP_RECV_THROTTLE_WIN) {
		base_rtt = rt->rtt_min;
		if (tp->t_rttcur <= (base_rtt + target_qdelay)) {
			tp->t_flagsext &= ~TF_RECV_THROTTLE;
			tp->t_recv_throttle_ts = 0;
		}
	}
#endif /* TRAFFIC_MGT */

	/* Update the next current RTT sample */
	update_curr_rtt(tp, rtt);

	if ((int)(tcp_now - rt->rtt_expire_ts) >=
	    TCP_RTT_HISTORY_EXPIRE_TIME) {
		RT_LOCK_SPIN(rt);
		/* check the condition again to avoid race */
		if ((int)(tcp_now - rt->rtt_expire_ts) >=
		    TCP_RTT_HISTORY_EXPIRE_TIME) {
			/* Set the base rtt to 0 for idle periods */
			uint32_t times = MIN((tcp_now - rt->rtt_expire_ts) /
			    TCP_RTT_HISTORY_EXPIRE_TIME, NRTT_HIST + 1);

			for (i = rt->rtt_index + 1; i < rt->rtt_index + times; i++) {
				rt->rtt_hist[i % NRTT_HIST] = 0;
			}

			rt->rtt_index = i % NRTT_HIST;
			rt->rtt_hist[rt->rtt_index] = rtt;
			rt->rtt_expire_ts = tcp_now;
		} else {
			rt->rtt_hist[rt->rtt_index] =
			    min(rt->rtt_hist[rt->rtt_index], rtt);
		}
		/* forget the old value and update minimum */
		rt->rtt_min = 0;
		for (i = 0; i < NRTT_HIST; ++i) {
			if (rt->rtt_hist[i] != 0 &&
			    (rt->rtt_min == 0 ||
			    rt->rtt_hist[i] < rt->rtt_min)) {
				rt->rtt_min = rt->rtt_hist[i];
			}
		}
		RT_UNLOCK(rt);
	} else {
		rt->rtt_hist[rt->rtt_index] =
		    min(rt->rtt_hist[rt->rtt_index], rtt);
		if (rt->rtt_min == 0) {
			rt->rtt_min = rtt;
		} else {
			rt->rtt_min = min(rt->rtt_min, rtt);
		}
	}
}

/*
 * If we have a timestamp reply, update smoothed RTT. If no timestamp is
 * present but transmit timer is running and timed sequence number was
 * acked, update smoothed RTT.
 *
 * If timestamps are supported, a receiver can update RTT even if
 * there is no outstanding data.
 *
 * Some boxes send broken timestamp replies during the SYN+ACK phase,
 * ignore timestamps of 0or we could calculate a huge RTT and blow up
 * the retransmit timer.
 */
static void
tcp_compute_rtt(struct tcpcb *tp, struct tcpopt *to, struct tcphdr *th)
{
	int rtt = 0;
	VERIFY(to != NULL && th != NULL);
	if (tp->t_rtttime != 0 && SEQ_GT(th->th_ack, tp->t_rtseq)) {
		u_int32_t pipe_ack_val;
		rtt = tcp_now - tp->t_rtttime;
		if (rtt == 0) {
			/*
			 * Make adjustment for sub ms RTT when
			 * timestamps are not used.
			 */
			rtt = 1;
		}
		/*
		 * Compute pipe ack -- the amount of data acknowledged
		 * in the last RTT -- only works for sender
		 */
		if (SEQ_GT(th->th_ack, tp->t_pipeack_lastuna)) {
			pipe_ack_val = th->th_ack - tp->t_pipeack_lastuna;
			/* Update the sample */
			tp->t_pipeack_sample[tp->t_pipeack_ind++] =
			    pipe_ack_val;
			tp->t_pipeack_ind %= TCP_PIPEACK_SAMPLE_COUNT;

			/* Compute the max of the pipeack samples */
			pipe_ack_val = tcp_get_max_pipeack(tp);
			tp->t_pipeack = (pipe_ack_val >
			    tcp_initial_cwnd(tp)) ?
			    pipe_ack_val : 0;
		}
		/* start another measurement */
		tp->t_rtttime = 0;
	}
	if (((to->to_flags & TOF_TS) != 0) &&
	    (to->to_tsecr != 0) &&
	    TSTMP_GEQ(tcp_now, to->to_tsecr)) {
		tcp_xmit_timer(tp, (tcp_now - to->to_tsecr),
		    to->to_tsecr, th->th_ack);
	} else if (rtt > 0) {
		tcp_xmit_timer(tp, rtt, 0, th->th_ack);
	}
}

static void
tcp_compute_rcv_rtt(struct tcpcb *tp, struct tcpopt *to, struct tcphdr *th)
{
	uint32_t rtt = 0, delta = 0;
	VERIFY(to != NULL && th != NULL);

	/* Calculate RTT */
	if (((to->to_flags & TOF_TS) != 0) && (to->to_tsecr != 0) &&
	    TSTMP_GEQ(tcp_now, to->to_tsecr)) {
		/* Timestamp is supported */
		rtt = tcp_now - to->to_tsecr;
		if (rtt == 0) {
			/* Make adjustment for sub ms RTT */
			rtt = 1;
		}
	} else if ((to->to_flags & TOF_TS) == 0) {
		/*
		 * Timestamp is not supported, 1RTT is roughly
		 * the time to receive one full window of data
		 * Currently, RTT calculated this way is only used
		 * for auto-tuning.
		 */
		if (tp->rcv_rtt_est_ts != 0) {
			if (SEQ_LT(tp->rcv_nxt, tp->rcv_rtt_est_seq)) {
				/* Haven't received a full window yet */
				return;
			} else {
				rtt = tcp_now - tp->rcv_rtt_est_ts;
				if (rtt == 0) {
					/* Make adjustment for sub ms RTT */
					rtt = 1;
				}
			}
		} else {
			/* Use default value when no RTT measurement */
			rtt = TCPTV_RCVNOTS_QUANTUM;
		}
		/* Restart the measurement */
		tp->rcv_rtt_est_ts = tcp_now;
		tp->rcv_rtt_est_seq = tp->rcv_nxt + tp->rcv_wnd;
	}

	/* Update receiver's SRTT */
	if (tp->rcv_srtt != 0) {
		/*
		 * Use the smoothed rtt formula,
		 * (srtt = rtt/8 + srtt*7/8) in fixed point
		 */
		delta = (rtt << TCP_DELTA_SHIFT)
		    - (tp->rcv_srtt >> (TCP_RTT_SHIFT - TCP_DELTA_SHIFT));

		if ((tp->rcv_srtt += delta) <= 0) {
			tp->rcv_srtt = 1;
		}
	} else {
		/* No previous measurement */
		tp->rcv_srtt = rtt << TCP_RTT_SHIFT;
	}

	/*
	 * For current RTT, base RTT and current RTT over k samples,
	 * we are using the same state for both sender and receiver
	 * as the most recent sample is always updated before any
	 * other processing, i.e. the sender will not end up with
	 * a high RTT due to the receiver.
	 */
	tp->t_rttcur = rtt;
	update_base_rtt(tp, rtt);
}

/*
 * Collect new round-trip time estimate and update averages and
 * current timeout.
 */
static void
tcp_xmit_timer(struct tcpcb *tp, int rtt,
    u_int32_t tsecr, tcp_seq th_ack)
{
	VERIFY(rtt >= 0);
	int delta;
	int old_srtt = tp->t_srtt;
	int old_rttvar = tp->t_rttvar;
	bool log_rtt = false;

	if (rtt == 0) {
		/*
		 * As rtt has millisecond precision,
		 * make adjustment for sub ms RTT
		 */
		rtt = 1;
	}

	if (rtt > 4 * TCPTV_MSL) {
		TCP_LOG(tp, "%s: rtt is %d - maxing it at 4 x MSL\n", __func__, rtt);
		/*
		 * We compute RTT either based on the time-to-ACK a packet,
		 * if TSval is disabled or based on the TSecr value.
		 * If there is a middlebox messing up the TSecr value, we can
		 * end up having HUGE rtt values, causing all kinds of problems.
		 * Let's protect against this by capping RTT to 4*MSL
		 * (60seconds).
		 */
		rtt = 4 * TCPTV_MSL;
	}

	/*
	 * On AWDL interface, the initial RTT measurement on SYN
	 * can be wrong due to peer caching. Avoid the first RTT
	 * measurement as it might skew up the RTO.
	 * <rdar://problem/28739046>
	 */
	if (tp->t_inpcb->inp_last_outifp != NULL &&
	    (tp->t_inpcb->inp_last_outifp->if_eflags & IFEF_AWDL) &&
	    th_ack == tp->iss + 1) {
		return;
	}

	if (tp->t_flagsext & TF_RECOMPUTE_RTT) {
		if (SEQ_GT(th_ack, tp->snd_una) &&
		    SEQ_LEQ(th_ack, tp->snd_max) &&
		    (tsecr == 0 ||
		    TSTMP_GEQ(tsecr, tp->t_badrexmt_time))) {
			/*
			 * We received a new ACK after a
			 * spurious timeout. Adapt retransmission
			 * timer as described in rfc 4015.
			 */
			tp->t_flagsext &= ~(TF_RECOMPUTE_RTT);
			tp->t_badrexmt_time = 0;
			tp->t_srtt = max(tp->t_srtt_prev, rtt);
			tp->t_srtt = tp->t_srtt << TCP_RTT_SHIFT;
			tp->t_rttvar = max(tp->t_rttvar_prev, (rtt >> 1));
			tp->t_rttvar = tp->t_rttvar << TCP_RTTVAR_SHIFT;

			if (tp->t_rttbest > (tp->t_srtt + tp->t_rttvar)) {
				tp->t_rttbest = tp->t_srtt + tp->t_rttvar;
			}

			goto compute_rto;
		} else {
			return;
		}
	}

	tcpstat.tcps_rttupdated++;
	tp->t_rttupdated++;

	tp->t_rttcur = rtt;
	update_base_rtt(tp, rtt);

	if (tp->t_srtt != 0) {
		/*
		 * srtt is stored as fixed point with 5 bits after the
		 * binary point (i.e., scaled by 32).  The following magic
		 * is equivalent to the smoothing algorithm in rfc793 with
		 * an alpha of .875 (srtt = rtt/8 + srtt*7/8 in fixed
		 * point).
		 *
		 * Freebsd adjusts rtt to origin 0 by subtracting 1
		 * from the provided rtt value. This was required because
		 * of the way t_rtttime was initiailised to 1 before.
		 * Since we changed t_rtttime to be based on
		 * tcp_now, this extra adjustment is not needed.
		 */
		delta = (rtt << TCP_DELTA_SHIFT)
		    - (tp->t_srtt >> (TCP_RTT_SHIFT - TCP_DELTA_SHIFT));

		if ((tp->t_srtt += delta) <= 0) {
			tp->t_srtt = 1;
		}

		/*
		 * We accumulate a smoothed rtt variance (actually, a
		 * smoothed mean difference), then set the retransmit
		 * timer to smoothed rtt + 4 times the smoothed variance.
		 * rttvar is stored as fixed point with 4 bits after the
		 * binary point (scaled by 16).  The following is
		 * equivalent to rfc793 smoothing with an alpha of .75
		 * (rttvar = rttvar*3/4 + |delta| / 4).  This replaces
		 * rfc793's wired-in beta.
		 */
		if (delta < 0) {
			delta = -delta;
		}
		delta -= tp->t_rttvar >> (TCP_RTTVAR_SHIFT - TCP_DELTA_SHIFT);
		if ((tp->t_rttvar += delta) <= 0) {
			tp->t_rttvar = 1;
		}
		if (tp->t_rttbest == 0 ||
		    tp->t_rttbest > (tp->t_srtt + tp->t_rttvar)) {
			tp->t_rttbest = tp->t_srtt + tp->t_rttvar;
		}
	} else {
		/*
		 * No rtt measurement yet - use the unsmoothed rtt.
		 * Set the variance to half the rtt (so our first
		 * retransmit happens at 3*rtt).
		 */
		tp->t_srtt = rtt << TCP_RTT_SHIFT;
		tp->t_rttvar = rtt << (TCP_RTTVAR_SHIFT - 1);
		tp->t_rttbest = tp->t_srtt + tp->t_rttvar;

		/* Initialize the receive SRTT */
		if (tp->rcv_srtt == 0) {
			tp->rcv_srtt = tp->t_srtt;
		}
	}

compute_rto:
	nstat_route_rtt(tp->t_inpcb->inp_route.ro_rt, tp->t_srtt,
	    tp->t_rttvar);

	/*
	 * the retransmit should happen at rtt + 4 * rttvar.
	 * Because of the way we do the smoothing, srtt and rttvar
	 * will each average +1/2 tick of bias.  When we compute
	 * the retransmit timer, we want 1/2 tick of rounding and
	 * 1 extra tick because of +-1/2 tick uncertainty in the
	 * firing of the timer.  The bias will give us exactly the
	 * 1.5 tick we need.  But, because the bias is
	 * statistical, we have to test that we don't drop below
	 * the minimum feasible timer (which is 2 ticks).
	 */
	TCPT_RANGESET(tp->t_rxtcur, TCP_REXMTVAL(tp),
	    max(tp->t_rttmin, rtt + 2), TCPTV_REXMTMAX,
	    TCP_ADD_REXMTSLOP(tp));

	/*
	 * We received an ack for a packet that wasn't retransmitted;
	 * it is probably safe to discard any error indications we've
	 * received recently.  This isn't quite right, but close enough
	 * for now (a route might have failed after we sent a segment,
	 * and the return path might not be symmetrical).
	 */
	tp->t_softerror = 0;

	if (log_rtt) {
		TCP_LOG_RTT_INFO(tp);
	}

	TCP_LOG_RTT_CHANGE(tp, old_srtt, old_rttvar);
}

static inline unsigned int
tcp_maxmtu(struct rtentry *rt)
{
	unsigned int maxmtu;
	int interface_mtu = 0;

	RT_LOCK_ASSERT_HELD(rt);
	interface_mtu = rt->rt_ifp->if_mtu;

	if (rt_key(rt)->sa_family == AF_INET &&
	    INTF_ADJUST_MTU_FOR_CLAT46(rt->rt_ifp)) {
		interface_mtu = IN6_LINKMTU(rt->rt_ifp);
		/* Further adjust the size for CLAT46 expansion */
		interface_mtu -= CLAT46_HDR_EXPANSION_OVERHD;
	}

	if (rt->rt_rmx.rmx_mtu == 0) {
		maxmtu = interface_mtu;
	} else {
		maxmtu = MIN(rt->rt_rmx.rmx_mtu, interface_mtu);
	}

	return maxmtu;
}

static inline unsigned int
tcp_maxmtu6(struct rtentry *rt)
{
	unsigned int maxmtu;
	struct nd_ifinfo *ndi = NULL;

	RT_LOCK_ASSERT_HELD(rt);
	if ((ndi = ND_IFINFO(rt->rt_ifp)) != NULL && !ndi->initialized) {
		ndi = NULL;
	}
	if (ndi != NULL) {
		lck_mtx_lock(&ndi->lock);
	}
	if (rt->rt_rmx.rmx_mtu == 0) {
		maxmtu = IN6_LINKMTU(rt->rt_ifp);
	} else {
		maxmtu = MIN(rt->rt_rmx.rmx_mtu, IN6_LINKMTU(rt->rt_ifp));
	}
	if (ndi != NULL) {
		lck_mtx_unlock(&ndi->lock);
	}

	return maxmtu;
}

unsigned int
get_maxmtu(struct rtentry *rt)
{
	unsigned int maxmtu = 0;

	RT_LOCK_ASSERT_NOTHELD(rt);

	RT_LOCK(rt);

	if (rt_key(rt)->sa_family == AF_INET6) {
		maxmtu = tcp_maxmtu6(rt);
	} else {
		maxmtu = tcp_maxmtu(rt);
	}

	RT_UNLOCK(rt);

	return maxmtu;
}

/*
 * Determine a reasonable value for maxseg size.
 * If the route is known, check route for mtu.
 * If none, use an mss that can be handled on the outgoing
 * interface without forcing IP to fragment; if bigger than
 * an mbuf cluster (MCLBYTES), round down to nearest multiple of MCLBYTES
 * to utilize large mbufs.  If no route is found, route has no mtu,
 * or the destination isn't local, use a default, hopefully conservative
 * size (usually 512 or the default IP max size, but no more than the mtu
 * of the interface), as we can't discover anything about intervening
 * gateways or networks.  We also initialize the congestion/slow start
 * window. While looking at the routing entry, we also initialize
 * other path-dependent parameters from pre-set or cached values
 * in the routing entry.
 *
 * Also take into account the space needed for options that we
 * send regularly.  Make maxseg shorter by that amount to assure
 * that we can send maxseg amount of data even when the options
 * are present.  Store the upper limit of the length of options plus
 * data in maxopd.
 *
 * NOTE that this routine is only called when we process an incoming
 * segment, for outgoing segments only tcp_mssopt is called.
 *
 */
void
tcp_mss(struct tcpcb *tp, int offer, unsigned int input_ifscope)
{
	struct rtentry *rt;
	struct ifnet *ifp;
	int rtt, mss;
	uint32_t bufsize;
	struct inpcb *inp;
	struct socket *so;
	int origoffer = offer;
	int isnetlocal = 0;
	int isipv6;
	int min_protoh;

	inp = tp->t_inpcb;

	so = inp->inp_socket;
	/*
	 * Nothing left to send after the socket is defunct or TCP is in the closed state
	 */
	if ((so->so_state & SS_DEFUNCT) || tp->t_state == TCPS_CLOSED) {
		return;
	}

	isipv6 = ((inp->inp_vflag & INP_IPV6) != 0) ? 1 : 0;
	min_protoh = isipv6 ? sizeof(struct ip6_hdr) + sizeof(struct tcphdr)
	    : sizeof(struct tcpiphdr);

	if (isipv6) {
		rt = tcp_rtlookup6(inp, input_ifscope);
	} else {
		rt = tcp_rtlookup(inp, input_ifscope);
	}
	isnetlocal = (tp->t_flags & TF_LOCAL);

	if (rt == NULL) {
		tp->t_maxopd = tp->t_maxseg = isipv6 ? tcp_v6mssdflt : tcp_mssdflt;
		return;
	}
	ifp = rt->rt_ifp;
	/*
	 * Slower link window correction:
	 * If a value is specificied for slowlink_wsize use it for
	 * PPP links believed to be on a serial modem (speed <128Kbps).
	 * Excludes 9600bps as it is the default value adversized
	 * by pseudo-devices over ppp.
	 */
	if (ifp->if_type == IFT_PPP && slowlink_wsize > 0 &&
	    ifp->if_baudrate > 9600 && ifp->if_baudrate <= 128000) {
		tp->t_flags |= TF_SLOWLINK;
	}

	/*
	 * Offer == -1 means that we didn't receive SYN yet. Use 0 then.
	 */
	if (offer == -1) {
		offer = rt->rt_rmx.rmx_filler[0];
	}
	/*
	 * Offer == 0 means that there was no MSS on the SYN segment,
	 * in this case we use tcp_mssdflt.
	 */
	if (offer == 0) {
		offer = isipv6 ? tcp_v6mssdflt : tcp_mssdflt;
	} else {
		/*
		 * Prevent DoS attack with too small MSS. Round up
		 * to at least minmss.
		 */
		offer = max(offer, tcp_minmss);
		/*
		 * Sanity check: make sure that maxopd will be large
		 * enough to allow some data on segments even is the
		 * all the option space is used (40bytes).  Otherwise
		 * funny things may happen in tcp_output.
		 */
		offer = max(offer, 64);
	}
	rt->rt_rmx.rmx_filler[0] = offer;

	/*
	 * While we're here, check if there's an initial rtt
	 * or rttvar.  Convert from the route-table units
	 * to scaled multiples of the slow timeout timer.
	 */
	if (tp->t_srtt == 0 && (rtt = rt->rt_rmx.rmx_rtt) != 0) {
		tcp_getrt_rtt(tp, rt);
	} else {
		tp->t_rttmin = isnetlocal ? tcp_TCPTV_MIN : TCPTV_REXMTMIN;
	}

	mss = (isipv6 ? tcp_maxmtu6(rt) : tcp_maxmtu(rt));

#if NECP
	// At this point, the mss is just the MTU. Adjust if necessary.
	mss = necp_socket_get_effective_mtu(inp, mss);
#endif /* NECP */

	mss -= min_protoh;

	if (rt->rt_rmx.rmx_mtu == 0) {
		if (isipv6) {
			if (!isnetlocal) {
				mss = min(mss, tcp_v6mssdflt);
			}
		} else if (!isnetlocal) {
			mss = min(mss, tcp_mssdflt);
		}
	}

	mss = min(mss, offer);
	/*
	 * maxopd stores the maximum length of data AND options
	 * in a segment; maxseg is the amount of data in a normal
	 * segment.  We need to store this value (maxopd) apart
	 * from maxseg, because now every segment carries options
	 * and thus we normally have somewhat less data in segments.
	 */
	tp->t_maxopd = mss;

	/*
	 * origoffer==-1 indicates, that no segments were received yet.
	 * In this case we just guess.
	 */
	if ((tp->t_flags & (TF_REQ_TSTMP | TF_NOOPT)) == TF_REQ_TSTMP &&
	    (origoffer == -1 ||
	    (tp->t_flags & TF_RCVD_TSTMP) == TF_RCVD_TSTMP)) {
		mss -= TCPOLEN_TSTAMP_APPA;
	}

#if MPTCP
	mss -= mptcp_adj_mss(tp, FALSE);
#endif /* MPTCP */
	tp->t_maxseg = mss;

	/*
	 * If there's a pipesize (ie loopback), change the socket
	 * buffer to that size only if it's bigger than the current
	 * sockbuf size.  Make the socket buffers an integral
	 * number of mss units; if the mss is larger than
	 * the socket buffer, decrease the mss.
	 */
#if RTV_SPIPE
	bufsize = rt->rt_rmx.rmx_sendpipe;
	if (bufsize < so->so_snd.sb_hiwat)
#endif
	bufsize = so->so_snd.sb_hiwat;
	if (bufsize < mss) {
		mss = bufsize;
	} else {
		bufsize = (((bufsize + mss - 1) / mss) * mss);
		(void)sbreserve(&so->so_snd, bufsize);
	}
	tp->t_maxseg = mss;

	ASSERT(tp->t_maxseg);

	/*
	 * Update MSS using recommendation from link status report. This is
	 * temporary
	 */
	tcp_update_mss_locked(so, ifp);

#if RTV_RPIPE
	bufsize = rt->rt_rmx.rmx_recvpipe;
	if (bufsize < so->so_rcv.sb_hiwat)
#endif
	bufsize = so->so_rcv.sb_hiwat;
	if (bufsize > mss) {
		bufsize = (((bufsize + mss - 1) / mss) * mss);
		(void)sbreserve(&so->so_rcv, bufsize);
	}

	set_tcp_stream_priority(so);

	if (rt->rt_rmx.rmx_ssthresh) {
		/*
		 * There's some sort of gateway or interface
		 * buffer limit on the path.  Use this to set
		 * slow-start threshold, but set the threshold to
		 * no less than 2*mss.
		 */
		tp->snd_ssthresh = max(2 * mss, rt->rt_rmx.rmx_ssthresh);
		tcpstat.tcps_usedssthresh++;
	} else {
		tp->snd_ssthresh = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	}

	/*
	 * Set the slow-start flight size depending on whether this
	 * is a local network or not.
	 */
	if (CC_ALGO(tp)->cwnd_init != NULL) {
		CC_ALGO(tp)->cwnd_init(tp);
	}

	tcp_ccdbg_trace(tp, NULL, TCP_CC_CWND_INIT);

	if (TCP_USE_RLEDBAT(tp, so) && tcp_cc_rledbat.rwnd_init != NULL) {
		tcp_cc_rledbat.rwnd_init(tp);
	}

	/* Route locked during lookup above */
	RT_UNLOCK(rt);
}

/*
 * Determine the MSS option to send on an outgoing SYN.
 */
int
tcp_mssopt(struct tcpcb *tp)
{
	struct rtentry *rt;
	int mss;
	int isipv6;
	int min_protoh;

	isipv6 = ((tp->t_inpcb->inp_vflag & INP_IPV6) != 0) ? 1 : 0;
	min_protoh = isipv6 ? sizeof(struct ip6_hdr) + sizeof(struct tcphdr)
	    : sizeof(struct tcpiphdr);

	if (isipv6) {
		rt = tcp_rtlookup6(tp->t_inpcb, IFSCOPE_NONE);
	} else {
		rt = tcp_rtlookup(tp->t_inpcb, IFSCOPE_NONE);
	}
	if (rt == NULL) {
		return isipv6 ? tcp_v6mssdflt : tcp_mssdflt;
	}
	/*
	 * Slower link window correction:
	 * If a value is specificied for slowlink_wsize use it for PPP links
	 * believed to be on a serial modem (speed <128Kbps). Excludes 9600bps as
	 * it is the default value adversized by pseudo-devices over ppp.
	 */
	if (rt->rt_ifp->if_type == IFT_PPP && slowlink_wsize > 0 &&
	    rt->rt_ifp->if_baudrate > 9600 && rt->rt_ifp->if_baudrate <= 128000) {
		tp->t_flags |= TF_SLOWLINK;
	}

	mss = (isipv6 ? tcp_maxmtu6(rt) : tcp_maxmtu(rt));
	/* Route locked during lookup above */
	RT_UNLOCK(rt);

#if NECP
	// At this point, the mss is just the MTU. Adjust if necessary.
	mss = necp_socket_get_effective_mtu(tp->t_inpcb, mss);
#endif /* NECP */

	return mss - min_protoh;
}

/*
 * On a partial ack arrives, force the retransmission of the
 * next unacknowledged segment.  Do not clear tp->t_dupacks.
 * By setting snd_nxt to th_ack, this forces retransmission timer to
 * be started again.
 */
static void
tcp_newreno_partial_ack(struct tcpcb *tp, struct tcphdr *th)
{
	tcp_seq onxt = tp->snd_nxt;
	u_int32_t  ocwnd = tp->snd_cwnd;
	tp->t_timer[TCPT_REXMT] = 0;
	tp->t_timer[TCPT_PTO] = 0;
	tp->t_rtttime = 0;
	tp->snd_nxt = th->th_ack;
	/*
	 * Set snd_cwnd to one segment beyond acknowledged offset
	 * (tp->snd_una has not yet been updated when this function
	 *  is called)
	 */
	tp->snd_cwnd = tp->t_maxseg + BYTES_ACKED(th, tp);
	(void) tcp_output(tp);
	tp->snd_cwnd = ocwnd;
	if (SEQ_GT(onxt, tp->snd_nxt)) {
		tp->snd_nxt = onxt;
	}
	/*
	 * Partial window deflation.  Relies on fact that tp->snd_una
	 * not updated yet.
	 */
	if (tp->snd_cwnd > BYTES_ACKED(th, tp)) {
		tp->snd_cwnd -= BYTES_ACKED(th, tp);
	} else {
		tp->snd_cwnd = 0;
	}
	tp->snd_cwnd += tp->t_maxseg;
}

/*
 * Drop a random TCP connection that hasn't been serviced yet and
 * is eligible for discard.  There is a one in qlen chance that
 * we will return a null, saying that there are no dropable
 * requests.  In this case, the protocol specific code should drop
 * the new request.  This insures fairness.
 *
 * The listening TCP socket "head" must be locked
 */
static int
tcp_dropdropablreq(struct socket *head)
{
	struct socket *so, *sonext;
	unsigned int j, qlen;
	static uint32_t rnd = 0;
	static uint64_t old_runtime;
	static unsigned int cur_cnt, old_cnt;
	uint64_t now_sec, i;
	struct inpcb *inp = NULL;
	struct tcpcb *tp;

	if ((head->so_options & SO_ACCEPTCONN) == 0) {
		return 0;
	}

	if (TAILQ_EMPTY(&head->so_incomp)) {
		return 0;
	}

	so_acquire_accept_list(head, NULL);
	socket_unlock(head, 0);

	/*
	 * Check if there is any socket in the incomp queue
	 * that is closed because of a reset from the peer and is
	 * waiting to be garbage collected. If so, pick that as
	 * the victim
	 */
	TAILQ_FOREACH_SAFE(so, &head->so_incomp, so_list, sonext) {
		inp = sotoinpcb(so);
		tp = intotcpcb(inp);
		if (tp != NULL && tp->t_state == TCPS_CLOSED &&
		    so->so_head != NULL &&
		    (so->so_state & (SS_INCOMP | SS_CANTSENDMORE | SS_CANTRCVMORE)) ==
		    (SS_INCOMP | SS_CANTSENDMORE | SS_CANTRCVMORE)) {
			/*
			 * The listen socket is already locked but we
			 * can lock this socket here without lock ordering
			 * issues because it is in the incomp queue and
			 * is not visible to others.
			 */
			if (socket_try_lock(so)) {
				so->so_usecount++;
				goto found_victim;
			} else {
				continue;
			}
		}
	}

	so = TAILQ_FIRST(&head->so_incomp);

	now_sec = net_uptime();
	if ((i = (now_sec - old_runtime)) != 0) {
		old_runtime = now_sec;
		old_cnt = cur_cnt / i;
		cur_cnt = 0;
	}

	qlen = head->so_incqlen;
	if (rnd == 0) {
		rnd = RandomULong();
	}

	if (++cur_cnt > qlen || old_cnt > qlen) {
		rnd = (314159 * rnd + 66329) & 0xffff;
		j = ((qlen + 1) * rnd) >> 16;

		while (j-- && so) {
			so = TAILQ_NEXT(so, so_list);
		}
	}
	/* Find a connection that is not already closing (or being served) */
	while (so) {
		inp = (struct inpcb *)so->so_pcb;

		sonext = TAILQ_NEXT(so, so_list);

		if (in_pcb_checkstate(inp, WNT_ACQUIRE, 0) != WNT_STOPUSING) {
			/*
			 * Avoid the issue of a socket being accepted
			 * by one input thread and being dropped by
			 * another input thread. If we can't get a hold
			 * on this mutex, then grab the next socket in
			 * line.
			 */
			if (socket_try_lock(so)) {
				so->so_usecount++;
				if ((so->so_usecount == 2) &&
				    (so->so_state & SS_INCOMP) &&
				    !(so->so_flags & SOF_INCOMP_INPROGRESS)) {
					break;
				} else {
					/*
					 * don't use if being accepted or
					 * used in any other way
					 */
					in_pcb_checkstate(inp, WNT_RELEASE, 1);
					socket_unlock(so, 1);
				}
			} else {
				/*
				 * do not try to lock the inp in
				 * in_pcb_checkstate because the lock
				 * is already held in some other thread.
				 * Only drop the inp_wntcnt reference.
				 */
				in_pcb_checkstate(inp, WNT_RELEASE, 1);
			}
		}
		so = sonext;
	}
	if (so == NULL) {
		socket_lock(head, 0);
		so_release_accept_list(head);
		return 0;
	}

	/* Makes sure socket is still in the right state to be discarded */

	if (in_pcb_checkstate(inp, WNT_RELEASE, 1) == WNT_STOPUSING) {
		socket_unlock(so, 1);
		socket_lock(head, 0);
		so_release_accept_list(head);
		return 0;
	}

found_victim:
	if (so->so_usecount != 2 || !(so->so_state & SS_INCOMP)) {
		/* do not discard: that socket is being accepted */
		socket_unlock(so, 1);
		socket_lock(head, 0);
		so_release_accept_list(head);
		return 0;
	}

	socket_lock(head, 0);
	TAILQ_REMOVE(&head->so_incomp, so, so_list);
	head->so_incqlen--;
	head->so_qlen--;
	so->so_state &= ~SS_INCOMP;
	so->so_flags |= SOF_OVERFLOW;
	so->so_head = NULL;
	so_release_accept_list(head);
	socket_unlock(head, 0);

	socket_lock_assert_owned(so);
	tp = sototcpcb(so);

	tcp_close(tp);
	if (inp->inp_wantcnt > 0 && inp->inp_wantcnt != WNT_STOPUSING) {
		/*
		 * Some one has a wantcnt on this pcb. Since WNT_ACQUIRE
		 * doesn't require a lock, it could have happened while
		 * we are holding the lock. This pcb will have to
		 * be garbage collected later.
		 * Release the reference held for so_incomp queue
		 */
		VERIFY(so->so_usecount > 0);
		so->so_usecount--;
		socket_unlock(so, 1);
	} else {
		/*
		 * Unlock this socket and leave the reference on.
		 * We need to acquire the pcbinfo lock in order to
		 * fully dispose it off
		 */
		socket_unlock(so, 0);

		lck_rw_lock_exclusive(&tcbinfo.ipi_lock);

		socket_lock(so, 0);
		/* Release the reference held for so_incomp queue */
		VERIFY(so->so_usecount > 0);
		so->so_usecount--;

		if (so->so_usecount != 1 ||
		    (inp->inp_wantcnt > 0 &&
		    inp->inp_wantcnt != WNT_STOPUSING)) {
			/*
			 * There is an extra wantcount or usecount
			 * that must have been added when the socket
			 * was unlocked. This socket will have to be
			 * garbage collected later
			 */
			socket_unlock(so, 1);
		} else {
			/* Drop the reference held for this function */
			VERIFY(so->so_usecount > 0);
			so->so_usecount--;

			in_pcbdispose(inp);
		}
		lck_rw_done(&tcbinfo.ipi_lock);
	}
	tcpstat.tcps_drops++;

	socket_lock(head, 0);
	return 1;
}

/* Set background congestion control on a socket */
void
tcp_set_background_cc(struct socket *so)
{
	tcp_set_new_cc(so, TCP_CC_ALGO_BACKGROUND_INDEX);
}

/* Set foreground congestion control on a socket */
void
tcp_set_foreground_cc(struct socket *so)
{
	if (tcp_use_newreno) {
		tcp_set_new_cc(so, TCP_CC_ALGO_NEWRENO_INDEX);
#if (DEVELOPMENT || DEBUG)
	} else if (tcp_use_ledbat) {
		/* Only used for testing */
		tcp_set_new_cc(so, TCP_CC_ALGO_BACKGROUND_INDEX);
#endif
	} else {
		struct inpcb *inp = sotoinpcb(so);
		struct tcpcb *tp = intotcpcb(inp);
		if (TCP_L4S_ENABLED(tp)) {
			tcp_set_new_cc(so, TCP_CC_ALGO_PRAGUE_INDEX);
		} else {
			tcp_set_new_cc(so, TCP_CC_ALGO_CUBIC_INDEX);
		}
	}
}

static void
tcp_set_new_cc(struct socket *so, uint8_t cc_index)
{
	struct inpcb *inp = sotoinpcb(so);
	struct tcpcb *tp = intotcpcb(inp);

	if (tp->tcp_cc_index != cc_index) {
		if (CC_ALGO(tp)->cleanup != NULL) {
			CC_ALGO(tp)->cleanup(tp);
		}
		tp->tcp_cc_index = cc_index;

		tcp_cc_allocate_state(tp);

		if (CC_ALGO(tp)->switch_to != NULL) {
			CC_ALGO(tp)->switch_to(tp);
		}

		tcp_ccdbg_trace(tp, NULL, TCP_CC_CHANGE_ALGO);
	}
}

void
tcp_set_recv_bg(struct socket *so)
{
	if (!IS_TCP_RECV_BG(so)) {
		so->so_flags1 |= SOF1_TRAFFIC_MGT_TCP_RECVBG;

		struct inpcb *inp = sotoinpcb(so);
		struct tcpcb *tp = intotcpcb(inp);

		if (TCP_RLEDBAT_ENABLED(tp) && tcp_cc_rledbat.switch_to != NULL) {
			tcp_cc_rledbat.switch_to(tp);
		}
	}
}

void
tcp_clear_recv_bg(struct socket *so)
{
	if (IS_TCP_RECV_BG(so)) {
		so->so_flags1 &= ~(SOF1_TRAFFIC_MGT_TCP_RECVBG);
	}
}

void
inp_fc_throttle_tcp(struct inpcb *inp)
{
	struct tcpcb *tp = inp->inp_ppcb;

	if (!tcp_flow_control_response) {
		return;
	}

	/*
	 * Back off the slow-start threshold and enter
	 * congestion avoidance phase
	 */
	if (CC_ALGO(tp)->pre_fr != NULL) {
		CC_ALGO(tp)->pre_fr(tp);
	}
}

void
inp_fc_unthrottle_tcp(struct inpcb *inp)
{
	struct tcpcb *tp = inp->inp_ppcb;

	if (tcp_flow_control_response) {
		if (CC_ALGO(tp)->post_fr != NULL) {
			CC_ALGO(tp)->post_fr(tp, NULL);
		}

		tp->t_bytes_acked = 0;

		/*
		 * Reset retransmit shift as we know that the reason
		 * for delay in sending a packet is due to flow
		 * control on the outgoing interface. There is no need
		 * to backoff retransmit timer.
		 */
		TCP_RESET_REXMT_STATE(tp);

		tp->t_flagsext &= ~TF_CWND_NONVALIDATED;

		/*
		 * Start the output stream again. Since we are
		 * not retransmitting data, do not reset the
		 * retransmit timer or rtt calculation.
		 */
		tcp_output(tp);
		return;
	}

	/*
	 * Back off the slow-start threshold and enter
	 * congestion avoidance phase
	 */
	if (CC_ALGO(tp)->pre_fr != NULL) {
		CC_ALGO(tp)->pre_fr(tp);
	}

	tp->snd_cwnd = tp->snd_ssthresh;
	tp->t_flagsext &= ~TF_CWND_NONVALIDATED;
	/*
	 * Restart counting for ABC as we changed the
	 * congestion window just now.
	 */
	tp->t_bytes_acked = 0;

	/* Reset retransmit shift as we know that the reason
	 * for delay in sending a packet is due to flow
	 * control on the outgoing interface. There is no need
	 * to backoff retransmit timer.
	 */
	TCP_RESET_REXMT_STATE(tp);

	/*
	 * Start the output stream again. Since we are
	 * not retransmitting data, do not reset the
	 * retransmit timer or rtt calculation.
	 */
	tcp_output(tp);
}

static int
tcp_getstat SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)

	int error;
	struct tcpstat *stat;
	stat = &tcpstat;

#if XNU_TARGET_OS_OSX
	struct tcpstat zero_stat;

	if (tcp_disable_access_to_stats &&
	    !kauth_cred_issuser(kauth_cred_get())) {
		bzero(&zero_stat, sizeof(zero_stat));
		stat = &zero_stat;
	}

#endif /* XNU_TARGET_OS_OSX */

	if (req->oldptr == 0) {
		req->oldlen = (size_t)sizeof(struct tcpstat);
	}

	error = SYSCTL_OUT(req, stat, MIN(sizeof(tcpstat), req->oldlen));

	return error;
}

/*
 * Checksum extended TCP header and data.
 */
int
tcp_input_checksum(int af, struct mbuf *m, struct tcphdr *th, int off, int tlen)
{
	struct ifnet *ifp = m->m_pkthdr.rcvif;

	switch (af) {
	case AF_INET: {
		struct ip *ip = mtod(m, struct ip *);
		struct ipovly *ipov = (struct ipovly *)ip;

		/* ip_stripoptions() must have been called before we get here */
		ASSERT((ip->ip_hl << 2) == sizeof(*ip));

		if ((hwcksum_rx || (ifp->if_flags & IFF_LOOPBACK) ||
		    (m->m_pkthdr.pkt_flags & PKTF_LOOP)) &&
		    (m->m_pkthdr.csum_flags & CSUM_DATA_VALID)) {
			if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR) {
				th->th_sum = m->m_pkthdr.csum_rx_val;
			} else {
				uint32_t sum = m->m_pkthdr.csum_rx_val;
				uint32_t start = m->m_pkthdr.csum_rx_start;
				int32_t trailer = (m_pktlen(m) - (off + tlen));

				/*
				 * Perform 1's complement adjustment of octets
				 * that got included/excluded in the hardware-
				 * calculated checksum value.  Ignore cases
				 * where the value already includes the entire
				 * IP header span, as the sum for those octets
				 * would already be 0 by the time we get here;
				 * IP has already performed its header checksum
				 * checks.  If we do need to adjust, restore
				 * the original fields in the IP header when
				 * computing the adjustment value.  Also take
				 * care of any trailing bytes and subtract out
				 * their partial sum.
				 */
				ASSERT(trailer >= 0);
				if ((m->m_pkthdr.csum_flags & CSUM_PARTIAL) &&
				    ((start != 0 && start != off) || trailer)) {
					uint32_t swbytes = (uint32_t)trailer;

					if (start < off) {
						ip->ip_len += sizeof(*ip);
#if BYTE_ORDER != BIG_ENDIAN
						HTONS(ip->ip_len);
						HTONS(ip->ip_off);
#endif /* BYTE_ORDER != BIG_ENDIAN */
					}
					/* callee folds in sum */
					sum = m_adj_sum16(m, start, off,
					    tlen, sum);
					if (off > start) {
						swbytes += (off - start);
					} else {
						swbytes += (start - off);
					}

					if (start < off) {
#if BYTE_ORDER != BIG_ENDIAN
						NTOHS(ip->ip_off);
						NTOHS(ip->ip_len);
#endif /* BYTE_ORDER != BIG_ENDIAN */
						ip->ip_len -= sizeof(*ip);
					}

					if (swbytes != 0) {
						tcp_in_cksum_stats(swbytes);
					}
					if (trailer != 0) {
						m_adj(m, -trailer);
					}
				}

				/* callee folds in sum */
				th->th_sum = in_pseudo(ip->ip_src.s_addr,
				    ip->ip_dst.s_addr,
				    sum + htonl(tlen + IPPROTO_TCP));
			}
			th->th_sum ^= 0xffff;
		} else {
			uint16_t ip_sum;
			int len;
			char b[9];

			bcopy(ipov->ih_x1, b, sizeof(ipov->ih_x1));
			bzero(ipov->ih_x1, sizeof(ipov->ih_x1));
			ip_sum = ipov->ih_len;
			ipov->ih_len = (u_short)tlen;
#if BYTE_ORDER != BIG_ENDIAN
			HTONS(ipov->ih_len);
#endif
			len = sizeof(struct ip) + tlen;
			th->th_sum = in_cksum(m, len);
			bcopy(b, ipov->ih_x1, sizeof(ipov->ih_x1));
			ipov->ih_len = ip_sum;

			tcp_in_cksum_stats(len);
		}
		break;
	}
	case AF_INET6: {
		struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);

		if ((hwcksum_rx || (ifp->if_flags & IFF_LOOPBACK) ||
		    (m->m_pkthdr.pkt_flags & PKTF_LOOP)) &&
		    (m->m_pkthdr.csum_flags & CSUM_DATA_VALID)) {
			if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR) {
				th->th_sum = m->m_pkthdr.csum_rx_val;
			} else {
				uint32_t sum = m->m_pkthdr.csum_rx_val;
				uint32_t start = m->m_pkthdr.csum_rx_start;
				int32_t trailer = (m_pktlen(m) - (off + tlen));

				/*
				 * Perform 1's complement adjustment of octets
				 * that got included/excluded in the hardware-
				 * calculated checksum value.  Also take care
				 * of any trailing bytes and subtract out their
				 * partial sum.
				 */
				ASSERT(trailer >= 0);
				if ((m->m_pkthdr.csum_flags & CSUM_PARTIAL) &&
				    (start != off || trailer != 0)) {
					uint16_t s = 0, d = 0;
					uint32_t swbytes = (uint32_t)trailer;

					if (IN6_IS_SCOPE_EMBED(&ip6->ip6_src)) {
						s = ip6->ip6_src.s6_addr16[1];
						ip6->ip6_src.s6_addr16[1] = 0;
					}
					if (IN6_IS_SCOPE_EMBED(&ip6->ip6_dst)) {
						d = ip6->ip6_dst.s6_addr16[1];
						ip6->ip6_dst.s6_addr16[1] = 0;
					}

					/* callee folds in sum */
					sum = m_adj_sum16(m, start, off,
					    tlen, sum);
					if (off > start) {
						swbytes += (off - start);
					} else {
						swbytes += (start - off);
					}

					if (IN6_IS_SCOPE_EMBED(&ip6->ip6_src)) {
						ip6->ip6_src.s6_addr16[1] = s;
					}
					if (IN6_IS_SCOPE_EMBED(&ip6->ip6_dst)) {
						ip6->ip6_dst.s6_addr16[1] = d;
					}

					if (swbytes != 0) {
						tcp_in6_cksum_stats(swbytes);
					}
					if (trailer != 0) {
						m_adj(m, -trailer);
					}
				}

				th->th_sum = in6_pseudo(
					&ip6->ip6_src, &ip6->ip6_dst,
					sum + htonl(tlen + IPPROTO_TCP));
			}
			th->th_sum ^= 0xffff;
		} else {
			tcp_in6_cksum_stats(tlen);
			th->th_sum = in6_cksum(m, IPPROTO_TCP, off, tlen);
		}
		break;
	}
	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	if (th->th_sum != 0) {
		tcpstat.tcps_rcvbadsum++;
		IF_TCP_STATINC(ifp, badformat);
		return -1;
	}

	return 0;
}

#define DUMP_BUF_CHK() {        \
	clen -= k;              \
	if (clen < 1)           \
	        goto done;      \
	c += k;                 \
}

int
dump_tcp_reass_qlen(char *str, int str_len)
{
	char *c = str;
	int k, clen = str_len;

	if (tcp_reass_total_qlen != 0) {
		k = scnprintf(c, clen, "\ntcp reass qlen %d\n", tcp_reass_total_qlen);
		DUMP_BUF_CHK();
	}

done:
	return str_len - clen;
}

uint32_t
tcp_reass_qlen_space(struct socket *so)
{
	uint32_t space = 0;
	struct inpcb *inp = sotoinpcb(so);

	if (inp != NULL) {
		struct tcpcb *tp = intotcpcb(inp);

		if (tp != NULL) {
			space = tp->t_reassq_mbcnt;
		}
	}
	return space;
}


SYSCTL_PROC(_net_inet_tcp, TCPCTL_STATS, stats,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED, 0, 0, tcp_getstat,
    "S,tcpstat", "TCP statistics (struct tcpstat, netinet/tcp_var.h)");

static int
sysctl_rexmtthresh SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)

	int error, val = tcprexmtthresh;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr) {
		return error;
	}

	/*
	 * Constrain the number of duplicate ACKs
	 * to consider for TCP fast retransmit
	 * to either 2 or 3
	 */

	if (val < 2 || val > 3) {
		return EINVAL;
	}

	tcprexmtthresh = (uint8_t)val;

	return 0;
}

SYSCTL_PROC(_net_inet_tcp, OID_AUTO, rexmt_thresh, CTLTYPE_INT | CTLFLAG_RW |
    CTLFLAG_LOCKED, &tcprexmtthresh, 0, &sysctl_rexmtthresh, "I",
    "Duplicate ACK Threshold for Fast Retransmit");
