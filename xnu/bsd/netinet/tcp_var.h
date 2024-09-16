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
 * Copyright (c) 1982, 1986, 1993, 1994, 1995
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
 *	@(#)tcp_var.h	8.4 (Berkeley) 5/24/95
 * $FreeBSD: src/sys/netinet/tcp_var.h,v 1.56.2.8 2001/08/22 00:59:13 silby Exp $
 */

#ifndef _NETINET_TCP_VAR_H_
#define _NETINET_TCP_VAR_H_
#include <sys/types.h>
#include <sys/appleapiopts.h>
#include <sys/queue.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#if !KERNEL
#include <TargetConditionals.h>
#endif

#if defined(__LP64__)
#define _TCPCB_PTR(x)                   u_int32_t
#define _TCPCB_LIST_HEAD(name, type)    \
struct name {                           \
	u_int32_t	lh_first;       \
}
#else
#define _TCPCB_PTR(x)                   x
#define _TCPCB_LIST_HEAD(name, type)    LIST_HEAD(name, type)
#endif

#ifdef KERNEL_PRIVATE
#include <sys/tree.h>

#define TCP_RETRANSHZ   1000    /* granularity of TCP timestamps, 1ms */
/* Minimum time quantum within which the timers are coalesced */
#define TCP_TIMER_10MS_QUANTUM  (TCP_RETRANSHZ/100) /* every 10ms */
#define TCP_TIMER_100MS_QUANTUM   (TCP_RETRANSHZ/10) /* every 100ms */
#define TCP_TIMER_500MS_QUANTUM   (TCP_RETRANSHZ/2) /* every 500ms */

#define TCP_RETRANSHZ_TO_USEC 1000

#define N_TIME_WAIT_SLOTS   128         /* must be power of 2 */

/* Always allow at least 16 packets worth of recv window when adjusting
 * recv window using inter-packet arrival jitter.
 */
#define MIN_IAJ_WIN 16

/* A variation in delay of this many milliseconds is tolerable. This limit has to
 * be low but greater than zero. We also use standard deviation on jitter to adjust
 * this limit for different link and connection types.
 */
#define ALLOWED_IAJ 5

/* Ignore the first few packets on a connection until the ACK clock gets going
 */
#define IAJ_IGNORE_PKTCNT 40

/* Let the accumulated IAJ value increase by this threshold at most. This limit
 * will control how many ALLOWED_IAJ measurements a receiver will have to see
 * before opening the receive window
 */
#define ACC_IAJ_HIGH_THRESH 100

/* When accumulated IAJ reaches this value, the receiver starts to react by
 * closing the window
 */
#define ACC_IAJ_REACT_LIMIT 200

/* If the number of small packets (smaller than IAJ packet size) seen on a
 * connection is more than this threshold, reset the size and learn it again.
 * This is needed because the sender might send smaller segments after PMTU
 * discovery and the receiver has to learn the new size.
 */
#define RESET_IAJ_SIZE_THRESH 20

/*
 * Adaptive timeout is a read/write timeout specified by the application to
 * get a socket event when the transport layer detects a stall in data
 * transfer. The value specified is the number of probes that can be sent
 * to the peer before generating an event. Since it is not specified as
 * a time value, the timeout will adjust based on the RTT seen on the link.
 * The timeout will start only when there is an indication that the read/write
 * operation is not making progress.
 *
 * If a write operation stalls, the probe will be retransmission of data.
 * If a read operation stalls, the probe will be a keep-alive packet.
 *
 * The maximum value of adaptive timeout is set to 10 which will allow
 * transmission of enough number of probes to the peer.
 */
#define TCP_ADAPTIVE_TIMEOUT_MAX        10

#define TCP_CONNECTIVITY_PROBES_MAX     5

/* 3 bit mask used for Accurate ECN ACE field */
#define TCP_ACE_MASK (0x7)
/* Divisor used for Accurate ECN ACE field */
#define TCP_ACE_DIV (1 << 3)

/* 24 bit mask used for Accurate ECN option */
#define TCP_ACO_MASK (0xFFFFFF)
/* Divisor used for Accurate ECN options field */
#define TCP_ACO_DIV (1 << 24)

/*
 * Kernel variables for tcp.
 */

/* TCP segment queue entry */
struct tseg_qent {
	LIST_ENTRY(tseg_qent) tqe_q;
	int     tqe_len;                /* TCP segment data length */
	struct  tcphdr *tqe_th;         /* a pointer to tcp header */
	struct  mbuf    *tqe_m;         /* mbuf contains packet */
};
LIST_HEAD(tsegqe_head, tseg_qent);

struct sackblk {
	tcp_seq start;          /* start seq no. of sack block */
	tcp_seq end;            /* end seq no. */
};

struct sackhole {
	tcp_seq start;          /* start seq no. of hole */
	tcp_seq end;            /* end seq no. */
	tcp_seq rxmit;          /* next seq. no in hole to be retransmitted */
	u_int32_t rxmit_start;  /* timestamp of first retransmission */
	TAILQ_ENTRY(sackhole) scblink;  /* scoreboard linkage */
};

struct sackhint {
	struct sackhole *nexthole;
	int     sack_bytes_rexmit;
	int sack_bytes_acked;
};

struct tcp_rxt_seg {
	tcp_seq rx_start;
	tcp_seq rx_end;
	u_int16_t rx_count;
	u_int16_t rx_flags;
#define TCP_RXT_SPURIOUS        0x1     /* received DSACK notification */
	SLIST_ENTRY(tcp_rxt_seg) rx_link;
};

/* TCP state of a segment that was sent */
struct tcp_seg_sent {
	tcp_seq start_seq;
	tcp_seq end_seq;        /* last seq sent + 1 */
	uint32_t xmit_ts;
	uint8_t flags;
	uint8_t pad[3];
#define TCP_SEGMENT_SACKED                  0x1
#define TCP_SEGMENT_LOST                    0x2
#define TCP_RACK_RETRANSMITTED              0x4
#define TCP_SEGMENT_RETRANSMITTED_ATLEAST_ONCE      0x8 /* If a segment was retransmitted at least once; used for reordering detection and to avoid spurious inferences for RACK.rtt */
#define TCP_SEGMENT_RETRANSMITTED           (TCP_RACK_RETRANSMITTED | TCP_SEGMENT_RETRANSMITTED_ATLEAST_ONCE)

	TAILQ_ENTRY(tcp_seg_sent) tx_link; /* Time ordered list of segments */
	RB_ENTRY(tcp_seg_sent)    seg_link; /* RB tree to keep track of S/ACKed segments */
	TAILQ_ENTRY(tcp_seg_sent) ack_link; /* List of ACKed segments to be deleted from time ordered list and RB tree */
	TAILQ_ENTRY(tcp_seg_sent) free_link; /* List of free segments from the pool */
};

typedef struct tcp_seg_pool {
	TAILQ_HEAD(, tcp_seg_sent) free_segs;
	uint32_t free_segs_count;
	char pad[4];
} *tcp_seg_pool_t;

#define TCP_SEG_POOL_MAX_ITEM_COUNT (64)

int tcp_seg_cmp(const struct tcp_seg_sent *, const struct tcp_seg_sent *);

RB_HEAD(tcp_seg_sent_tree_head, tcp_seg_sent);
RB_PROTOTYPE(tcp_seg_sent_tree_head, tcp_seg_sent, seg_link, tcp_seg_cmp)

struct tcp_notify_ack_marker {
	tcp_seq notify_snd_una; /* Notify when snd_una crosses this seq */
	tcp_notify_ack_id_t notify_id;
	SLIST_ENTRY(tcp_notify_ack_marker) notify_next;
};

struct tcptemp {
	u_char  tt_ipgen[40]; /* the size must be of max ip header, now IPv6 */
	struct  tcphdr tt_t;
};

struct bwmeas {
	tcp_seq bw_start;       /* start of bw measurement */
	uint32_t bw_ts;         /* timestamp when bw measurement started */
	uint32_t bw_size;       /* burst size in bytes for this bw measurement */
	uint32_t bw_minsizepkts; /* Min burst size as segments */
	uint32_t bw_maxsizepkts; /* Max burst size as segments */
	uint32_t bw_minsize;    /* Min size in bytes */
	uint32_t bw_maxsize;    /* Max size in bytes */
	uint32_t bw_sndbw;      /* Measured send bandwidth */
	uint32_t bw_sndbw_max;  /* Max measured bandwidth */
	uint32_t bw_rcvbw_max;  /* Max receive bandwidth measured */
};

/* MPTCP Data sequence map entry */
struct mpt_dsn_map {
	uint64_t                mpt_dsn;        /* data seq num recvd */
	uint32_t                mpt_sseq;       /* relative subflow # */
	uint16_t                mpt_len;        /* length of mapping */
	uint16_t                mpt_csum;       /* checksum value if on */
	uint8_t                 mpt_dfin;       /* It's a DATA_FIN */
};
#define tcp6cb          tcpcb  /* for KAME src sync over BSD*'s */

struct tcp_ccstate {
	union {
		struct tcp_cubic_state {
			u_int32_t tc_last_max;  /* cwnd at last loss */
			u_int32_t tc_epoch_start; /* TS of last loss */
			u_int32_t tc_origin_point; /* window at the start of an epoch */
			u_int32_t tc_tcp_win; /* computed tcp win */
			u_int32_t tc_tcp_bytes_acked; /* bytes acked */
			u_int32_t tc_avg_lastmax; /* Average of last max */
			u_int32_t tc_mean_deviation; /* Mean absolute deviation */
			float     tc_epoch_period; /* K parameter */
		} _cubic_state_;
#define cub_last_max __u__._cubic_state_.tc_last_max
#define cub_epoch_start __u__._cubic_state_.tc_epoch_start
#define cub_origin_point __u__._cubic_state_.tc_origin_point
#define cub_tcp_win __u__._cubic_state_.tc_tcp_win
#define cub_tcp_bytes_acked __u__._cubic_state_.tc_tcp_bytes_acked
#define cub_epoch_period __u__._cubic_state_.tc_epoch_period
#define cub_avg_lastmax __u__._cubic_state_.tc_avg_lastmax
#define cub_mean_dev __u__._cubic_state_.tc_mean_deviation
		struct tcp_prague_state {
			uint16_t num_cong_events_loss;
			uint16_t num_cong_events_ce;
			uint32_t packets_acked;   /* cumulative packets ack'ed updated at round start for alpha */
			uint32_t packets_marked;  /* cumulative CE counts updated at round start for alpha */
			uint32_t ce_counter;      /* cumulative CE counts updated on every ACK,
			                           *                        used for calculating non-CE acked and performing any CE related action */
			uint32_t bytes_acked;     /* bytes acked in this ACK that were not CE marked, used for cwnd increase */
			uint32_t snd_nxt_alpha;   /* snd_nxt at round start to track 1 RTT elapsed for alpha */
			uint32_t snd_nxt_cwr;     /* snd_nxt at round start to track 1 RTT elapsed for CWR */
			uint8_t ever_saw_ce:1,       /* To test if there is a AQM on path */
			    in_loss:1,                       /* Are we in loss recovery */
			    reduced_due_to_ce:1,             /* Was the last reduction due to CE */
			    unused:5;
			uint8_t pad[3];
			uint64_t scaled_alpha;    /* Scaled value of DCTCP.alpha */
			uint64_t alpha_ai;        /* AI.alpha used after CE for additive increase */
			/* After reduction due to loss CUBIC window increase state */
			struct tcp_cubic_state cubic_state;
		} _prague_state_;
#define num_cong_events_loss __u__._prague_state_.num_cong_events_loss
#define num_cong_events_ce __u__._prague_state_.num_cong_events_ce
#define prague_packets_acked __u__._prague_state_.packets_acked
#define prague_packets_marked __u__._prague_state_.packets_marked
#define prague_ce_counter __u__._prague_state_.ce_counter
#define prague_bytes_acked __u__._prague_state_.bytes_acked
#define snd_nxt_alpha __u__._prague_state_.snd_nxt_alpha
#define snd_nxt_cwr __u__._prague_state_.snd_nxt_cwr
#define prague_scaled_alpha __u__._prague_state_.scaled_alpha
#define prague_alpha_ai __u__._prague_state_.alpha_ai
#define ever_saw_ce __u__._prague_state_.ever_saw_ce
#define in_loss __u__._prague_state_.in_loss
#define reduced_due_to_ce __u__._prague_state_.reduced_due_to_ce
#define cubic_K __u__._prague_state_.cubic_state.tc_epoch_period
#define cubic_epoch_start __u__._prague_state_.cubic_state.tc_epoch_start
#define cubic_origin_point __u__._prague_state_.cubic_state.tc_origin_point
#define cubic_W_max __u__._prague_state_.cubic_state.tc_last_max
#define reno_acked __u__._prague_state_.cubic_state.tc_tcp_bytes_acked
#define reno_cwnd __u__._prague_state_.cubic_state.tc_tcp_win
		struct tcp_ledbat_state {
			uint32_t num_slowdown_events;
			uint32_t slowdown_ts;
			uint32_t slowdown_begin;
			uint32_t md_bytes_acked;
		} _ledbat_state_;
#define ledbat_slowdown_events __u__._ledbat_state_.num_slowdown_events
#define ledbat_slowdown_ts __u__._ledbat_state_.slowdown_ts
#define ledbat_slowdown_begin __u__._ledbat_state_.slowdown_begin
#define ledbat_md_bytes_acked __u__._ledbat_state_.md_bytes_acked
	} __u__;
};

struct tcp_rledbat_state {
	uint32_t num_slowdown_events;  /* Number of slowdown events until reset */
	uint32_t slowdown_ts;          /* Next slowdown timestamp */
	uint32_t slowdown_begin;       /* Slowdown begin time */
	uint32_t reduction_end;        /* Used to track rl_win reduction once per RTT */
	uint32_t rcvd_bytes;           /* bytes received used for byte counting */
	uint32_t md_rcvd_bytes;        /* bytes received during MD used for byte counting */
	uint32_t win;                  /* receive Ledbat window size */
	uint32_t ssthresh;             /* receive Ledbat ssthresh */
	uint32_t drained_bytes;        /* bytes drained from the flight-size */
	uint32_t win_ws;               /* receive Ledbat window after avoiding window shrinking */
};

struct accecn {
	/* ACE CE packet counters */
	uint32_t        t_rcv_ce_packets;                  /* Number of CE received packets at the receiver */
	uint32_t        t_snd_ce_packets;                  /* Synced number of CE received feedback at the sender */
	uint32_t        t_delta_ce_packets;                /* Change in CE count between previous and current ACK */
	uint8_t         accecn_processed:1,                /* Accurate ECN feedback has already been processed */
	    unused:7;

	/* AccECN option byte counters */
	uint64_t        t_rcv_ect1_bytes;                  /* ECT1 byte counter at the receiver, used for AccECN option feedback */
	uint64_t        t_rcv_ect0_bytes;                  /* ECT0 byte counter at the receiver, used for AccECN option feedback */
	uint64_t        t_rcv_ce_bytes;                    /* CE byte counter at the receiver, used for AccECN option feedback */
	uint64_t        t_snd_ect1_bytes;                  /* Synced ECT1 byte counter at the sender */
	uint64_t        t_snd_ect0_bytes;                  /* Synced ECT0 byte counter at the sender */
	uint64_t        t_snd_ce_bytes;                    /* Synced CE byte counter at the sender */
};

struct pacer {
	uint64_t rate;
	uint32_t tso_burst_size;
	uint64_t packet_tx_time;
};

/*
 * Tcp control block, one per tcp; fields:
 * Organized for 16 byte cacheline efficiency.
 */
struct tcpcb {
	struct tsegqe_head t_segq;
	uint32_t t_dupacks;             /* consecutive dup acks recd */
	int      t_state;               /* state of this connection */
	uint32_t t_timer[TCPT_NTIMERS]; /* tcp timers */
	struct tcptimerentry tentry;    /* entry in timer list */

	struct  inpcb *t_inpcb;         /* back pointer to internet pcb */
	uint32_t        t_flags;
#define TF_ACKNOW       0x00001         /* ack peer immediately */
#define TF_DELACK       0x00002         /* ack, but try to delay it */
#define TF_NODELAY      0x00004         /* don't delay packets to coalesce */
#define TF_NOOPT        0x00008         /* don't use tcp options */
#define TF_SENTFIN      0x00010         /* have sent FIN */
#define TF_REQ_SCALE    0x00020         /* have/will request window scaling */
#define TF_RCVD_SCALE   0x00040         /* other side has requested scaling */
#define TF_REQ_TSTMP    0x00080         /* have/will request timestamps */
#define TF_RCVD_TSTMP   0x00100         /* a timestamp was received in SYN */
#define TF_SACK_PERMIT  0x00200         /* other side said I could SACK */
#define TF_NEEDSYN      0x00400         /* send SYN (implicit state) - unused but needed for backwards compatibility */
#define TF_NEEDFIN      0x00800         /* send FIN (implicit state) */
#define TF_NOPUSH       0x01000         /* don't push */
#define TF_UNUSED1      0x02000         /* Unused */
#define TF_UNUSED2      0x04000         /* Unused */
#define TF_UNUSED3      0x08000         /* Unused */
#define TF_MORETOCOME   0x10000         /* More data to be appended to sock */
#define TF_LOCAL        0x20000         /* connection to a host on local link */
#define TF_RXWIN0SENT   0x40000         /* sent a receiver win 0 in response */
#define TF_SLOWLINK     0x80000         /* route is a on a modem speed link */
#define TF_LASTIDLE     0x100000        /* connection was previously idle */
#define TF_FASTRECOVERY 0x200000        /* in NewReno Fast Recovery */
#define TF_WASFRECOVERY 0x400000        /* was in NewReno Fast Recovery */
#define TF_SIGNATURE    0x800000        /* require MD5 digests (RFC2385) */
#define TF_MAXSEGSNT    0x1000000       /* last segment sent was a full segment */
#define TF_STREAMING_ON 0x2000000       /* Receiver detected streaming */
#define TF_PMTUD        0x4000000       /* Perform Path MTU Discovery for this connection */
#define TF_CLOSING      0x8000000       /* pending tcp close */
#define TF_TSO          0x10000000      /* TCP Segment Offloading is enable on this connection */
#define TF_BLACKHOLE    0x20000000      /* Path MTU Discovery Black Hole detection */
#define TF_TIMER_ONLIST 0x40000000      /* pcb is on tcp_timer_list */
#define TF_STRETCHACK   0x80000000      /* receiver is going to delay acks */

	tcp_seq snd_una;                /* send unacknowledged */
	tcp_seq snd_max;                /* highest sequence number sent;
	                                 * used to recognize retransmits
	                                 */
	tcp_seq snd_nxt;                /* send next */
	tcp_seq snd_up;                 /* send urgent pointer */

	tcp_seq snd_wl1;                /* window update seg seq number */
	tcp_seq snd_wl2;                /* window update seg ack number */
	tcp_seq iss;                    /* initial send sequence number */
	tcp_seq irs;                    /* initial receive sequence number */

	tcp_seq rcv_nxt;                /* receive next */
	tcp_seq rcv_adv;                /* advertised window */
	uint32_t        rcv_wnd;        /* receive window */
	uint32_t        t_last_recwin;
	tcp_seq rcv_up;                 /* receive urgent pointer */

	uint32_t        snd_wnd;        /* send window */
	uint32_t        snd_cwnd;       /* congestion-controlled window */
	uint32_t        snd_ssthresh;   /* snd_cwnd size threshold for
	                                 * for slow start exponential to
	                                 * linear switch
	                                 */
	tcp_seq snd_recover;            /* for use in NewReno Fast Recovery */

	uint32_t        t_maxopd;       /* mss plus options */
	uint32_t        t_rcvtime;      /* time at which a packet was received */
	uint32_t        t_sndtime;      /* time at which we last sent new data */
	uint32_t        t_starttime;    /* time connection was established */
	int     t_rtttime;              /* tcp clock when rtt calculation was started */
	tcp_seq t_rtseq;                /* sequence number being timed */

	uint32_t rfbuf_ts;              /* recv buffer autoscaling timestamp */
	uint32_t rfbuf_cnt;             /* recv buffer autoscaling byte count */
	uint32_t rfbuf_space;           /* Current "ideal" estimate of the space */

	int            t_rxtcur;        /* current retransmit value (ticks) */
	unsigned int   t_maxseg;        /* maximum segment size */
	int            t_srtt;          /* smoothed round-trip time */
	int            t_rttvar;        /* variance in round-trip time */

	uint64_t t_accsleep_ms;         /* accumulated sleep time since last boot */
	uint32_t t_reassqlen;           /* length of reassembly queue */
	uint32_t t_reassq_mbcnt;        /* amount in bytes of mbuf space used */
	uint16_t t_rxtshift;            /* log(2) of rexmt exp. backoff */
	uint32_t t_rttmin;              /* minimum rtt allowed */
	uint32_t t_rttbest;             /* best rtt we've seen */
	uint32_t t_rttcur;              /* most recent value of rtt */
	uint32_t t_rttupdated;          /* number of times rtt sampled */
	uint32_t t_rxt_conndroptime;    /* retxmt conn gets dropped after this time, when set */
	uint32_t t_rxtstart;            /* time at which retransmission started */
	uint32_t max_sndwnd;            /* largest window peer has offered */

	int     t_softerror;            /* possible error not yet reported */
/* out-of-band data */
	char    t_oobflags;             /* have some */
	char    t_iobc;                 /* input character */
#define TCPOOB_HAVEDATA 0x01
#define TCPOOB_HADDATA  0x02
/* RFC 1323 variables */
	u_int8_t        snd_scale;      /* window scaling for send window */
	u_int8_t        rcv_scale;      /* window scaling for recv window */
	u_int8_t        request_r_scale; /* pending window scaling */
	u_int8_t        requested_s_scale;
	u_int8_t        tcp_cc_index;   /* index of congestion control algorithm */
	u_int8_t        t_adaptive_rtimo;       /* Read timeout used as a multiple of RTT */
	u_int8_t        t_adaptive_wtimo;       /* Write timeout used as a multiple of RTT */
	u_int8_t        t_stretchack_delayed;   /* stretch ack delayed */

/* State for limiting early retransmits when SACK is not enabled */
	u_int16_t       t_early_rexmt_count; /* count of early rexmts */
	u_int32_t       t_early_rexmt_win; /* window for limiting early rexmts */

	u_int32_t       ts_recent;      /* timestamp echo data */

	u_int32_t       ts_recent_age;  /* when last updated */
	tcp_seq last_ack_sent;

	uint32_t        t_bytes_acked;  /* RFC 3465 variable for ABC, used by CCA only */
	uint32_t        total_ect_packets_marked; /* Cumulative count of total ECT packets marked */
	uint32_t        total_ect_packets_acked;  /* Cumulative count of total ECT packets acked */

	int             t_lastchain;    /* amount of packets chained last time around */
	uint16_t        t_unacksegs;    /* received but unacked segments for delaying acks */
	uint16_t        t_unacksegs_ce; /* received but unacked segments/pure ACKs that were CE marked */

	/*
	 * Pretty arbitrary value ;-)
	 * Goal is to make sure that some ACKs are being sent more frequently
	 * to allow the other side to ramp-up.
	 */
#define TCP_FORCED_ACKS_COUNT 16
	uint16_t        t_forced_acks;  /* count of pure ACKs that need to be forced out */
	uint8_t         t_rexmtthresh;  /* duplicate ack threshold for entering fast recovery */
	uint8_t         t_rtimo_probes; /* number of adaptive rtimo probes sent */
	uint32_t        t_persist_timeout; /* ZWP persistence limit as set by PERSIST_TIMEOUT */
	uint32_t        t_persist_stop;    /* persistence limit deadline if triggered by ZWP */
	uint32_t        t_notsent_lowat;   /* Low water for not sent data */

/* Receiver state for stretch-ack algorithm */
	u_int32_t       rcv_unackwin;   /* to measure win for stretching acks */
	u_int32_t       rcv_by_unackwin; /* bytes seen during the last ack-stretching win */
	u_int32_t       rcv_by_unackhalfwin;
	u_int32_t       rcv_nostrack_ts; /* timestamp when stretch ack was disabled automatically */
	u_int32_t       rcv_nostrack_pkts; /* pkts received since strech ack was disabled */
	u_int16_t       rcv_waitforss;  /* wait for packets during slow-start */

/* ECN stats */
	u_int32_t       ecn_flags;
#define TE_SETUPSENT            0x00000001  /* We have sent classic ECN-SETUP SYN or SYN-ACK */
#define TE_SETUPRECEIVED        0x00000002  /* We have received classic ECN-SETUP SYN or SYN-ACK */
#define TE_SENDIPECT            0x00000004  /* We haven't sent or received non-ECN-setup SYN or SYN-ACK, set IP ECT on outbound packet */
#define TE_SENDCWR              0x00000008  /* Next non-retransmit should have TCP CWR set, only used for classic ECN */
#define TE_SENDECE              0x00000010  /* Next packet should have TCP ECE set, only used for classic ECN */
#define TE_INRECOVERY           0x00000020  /* connection entered recovery after receiving ECE */
#define TE_RECV_ECN_CE          0x00000040  /* Received IPTOS_ECN_CE marking atleast once */
#define TE_RECV_ECN_ECE         0x00000080  /* Received ECE marking atleast once */
#define TE_LOST_SYN             0x00000100  /* Lost SYN with ECN setup */
#define TE_LOST_SYNACK          0x00000200  /* Lost SYN-ACK with ECN setup */
#define TE_ECN_MODE_ENABLE      0x00000400  /* Option ECN mode set to enable */
#define TE_ECN_MODE_DISABLE     0x00000800  /* Option ECN mode set to disable */
#define TE_ENABLE_ECN           0x00001000  /* Enable negotiation of ECN */
#define TE_ECN_ON               (TE_SETUPSENT | TE_SETUPRECEIVED) /* ECN was successfully negotiated on a connection */
#define TE_CEHEURI_SET          0x00002000  /* We did our CE-probing at the beginning */
#define TE_CLIENT_SETUP         0x00004000  /* setup from client side */
#define TE_RCVD_SYN_RST         0x00008000  /* Received RST to the first ECN enabled SYN */
#define TE_ACE_SETUP_NON_ECT    0x00010000  /* Encode received non-ECT either for SYN-ACK (server) or final ACK (client) */
#define TE_ACE_SETUP_ECT1       0x00020000  /* Encode received ECT1 either for SYN-ACK (server) or final ACK (client) */
#define TE_ACE_SETUP_ECT0       0x00040000  /* Encode received ECT0 either for SYN-ACK (server) or final ACK (client) */
#define TE_ACE_SETUP_CE         0x00080000  /* Encode received CE either for SYN-ACK (server) or final ACK (client) */
#define TE_ACE_SETUPSENT        0x00100000  /* We have sent Accurate ECN setup SYN or SYN-ACK */
#define TE_ACE_SETUPRECEIVED    0x00200000  /* We have received Accurate ECN setup SYN or SYN-ACK */
#define TE_ACC_ECN_ON          (TE_ACE_SETUPSENT | TE_ACE_SETUPRECEIVED) /* Accurate ECN was negotiated */
#define TE_ACE_FINAL_ACK_3WHS   0x00400000  /* Client has received SYN-ACK and will now send final ACK of 3WHS, only used for AccECN */
#define TE_ACO_ECT1             0x00800000  /* ECT1 counter changed flag, used to decide ordering for AccECN option */
#define TE_ACO_ECT0             0x01000000  /* ECT0 counter changed flag, used to decide ordering for AccECN option */
#define TE_RETRY_WITHOUT_ACO    0x02000000  /* Data segment with AccECN option was not acknowledged, retry without AccECN option */
#define TE_FORCE_ECT1           0x40000000  /* Force setting ECT1 on outgoing packets for testing purpose */
#define TE_FORCE_ECT0           0x80000000  /* Force setting ECT0 on outgoing packets for testing purpose */

	u_int32_t       t_ecn_recv_ce;  /* Received CE from the network */
	u_int32_t       t_ecn_recv_cwr; /* Packets received with CWR */
	uint32_t        t_client_accecn_state;    /* Client's Accurate ECN state */
	uint32_t        t_server_accecn_state;    /* Server's Accurate ECN state */
	uint64_t        t_ecn_capable_packets_sent;     /* Packets sent with ECT */
	uint64_t        t_ecn_capable_packets_acked;    /* Packets sent with ECT that were acked */
	uint64_t        t_ecn_capable_packets_marked;   /* Packets sent with ECT that were marked */
	uint64_t        t_ecn_capable_packets_lost;     /* Packets sent with ECT that were lost */

	uint32_t        t_last_ack_tsecr;       /* TS Echo Reply for last ACK that acknowledged a data or control packet */
	uint16_t        t_prev_ace_flags;   /* ACE flags that were sent in previous packet, used for retransmitting after timeout */
	uint8_t         t_prev_ip_ecn;      /* IP ECN flag on the previous packet, used for change-triggered ACKs */

	struct accecn   t_aecn;         /* AccECN related byte counters */

	struct pacer    t_pacer;        /* Pacer state used to pace packets */

/* state for bad retransmit recovery */
	u_int32_t       snd_cwnd_prev;  /* cwnd prior to retransmit */
	u_int32_t       snd_ssthresh_prev; /* ssthresh prior to retransmit */
	tcp_seq snd_recover_prev;       /* snd_recover prior to retransmit */
	int     t_srtt_prev;            /* srtt prior to retransmit */
	int     t_rttvar_prev;          /* rttvar prior to retransmit */
	u_int32_t       t_badrexmt_time; /* bad rexmt detection time */

/* Packet reordering metric */
	u_int32_t       t_reorderwin; /* Reordering late time offset */

/* SACK related state */
	int16_t snd_numholes;           /* number of holes seen by sender */
	TAILQ_HEAD(sackhole_head, sackhole) snd_holes;
	/* SACK scoreboard (sorted) */
	tcp_seq snd_fack;               /* last seq number(+1) sack'd by rcv'r*/
	int     rcv_numsacks;           /* # distinct sack blks present */
	struct sackblk sackblks[MAX_SACK_BLKS]; /* seq nos. of sack blocks */
	struct sackhint sackhint;       /* SACK scoreboard hint */

	struct mbuf     *t_pktlist_head; /* First packet in transmit chain */
	struct mbuf     *t_pktlist_tail; /* Last packet in transmit chain */
	u_int32_t       t_pktlist_sentlen; /* total bytes in transmit chain */

	u_int32_t       t_keepidle;     /* keepalive idle timer (override global if > 0) */
	u_int32_t       t_keepinit;     /* connection timeout, i.e. idle time
	                                 *  in SYN_SENT or SYN_RECV state */
	u_int32_t       t_keepintvl;    /* interval between keepalives */
	u_int32_t       t_keepcnt;      /* number of keepalives before close */

	u_int32_t       tso_max_segment_size;   /* TSO maximum segment unit for NIC */
	u_int16_t       t_pmtud_lastseg_size;   /* size of the last sent segment */
	u_int32_t       t_pmtud_saved_maxopd;   /* MSS saved before performing PMTU-D BlackHole detection */
	u_int32_t       t_pmtud_start_ts;       /* Time of PMTUD blackhole detection */

	struct{
		u_int32_t       rxduplicatebytes;
		u_int32_t       rxoutoforderbytes;
		u_int32_t       txretransmitbytes;
		u_int16_t       synrxtshift;
		u_int16_t       rxmitsyns;
		u_int16_t       unused_pad_to_8;
		u_int32_t       rxmitpkts;
		uint32_t        delayed_acks_sent;
		uint32_t        acks_delayed;
	} t_stat;
	u_int8_t        t_syn_sent;
	u_int8_t        t_syn_rcvd;
	u_int8_t        t_notify_ack_count;
	u_int8_t        t_ecn_recv_ce_pkt; /* Received packet with CE-bit set (independent from last_ack_sent) */
	u_int32_t       t_cached_maxopd; /* default for MSS adjustment using link status report */

	uint32_t        bg_ssthresh;            /* Slow start threshold until delay increases */
	uint32_t        t_flagsext;             /* Another field to accommodate more flags */
#define TF_RXTFINDROP           0x1                     /* Drop conn after retransmitting FIN 3 times */
#define TF_RCVUNACK_WAITSS      0x2             /* set when the receiver should not stretch acks */
#define TF_BWMEAS_INPROGRESS    0x4             /* Indicate BW meas is happening */
#define TF_MEASURESNDBW         0x8             /* Measure send bw on this connection */
#define TF_LAST_IS_PSH          0x10            /* Indicates whether the last packet in the rcv socket buffer had the PUSH-flag set */
#define TF_SACK_ENABLE          0x20            /* SACK is enabled */
#define TF_RECOMPUTE_RTT        0x40            /* recompute RTT after spurious retransmit */
#define TF_DETECT_READSTALL     0x80            /* Used to detect a stall during read operation */
#define TF_RECV_THROTTLE        0x100           /* Input throttling active */
#define TF_NOSTRETCHACK         0x200           /* ack every other packet */
#define TF_NOTIMEWAIT           0x800           /* Avoid going into time-wait */
#define TF_SENT_TLPROBE         0x1000          /* Sent data in PTO */
#define TF_PKTS_REORDERED       0x2000          /* Detected reordering */
#define TF_DELAY_RECOVERY       0x4000          /* delay fast recovery */
#define TF_FORCE                0x8000          /* force 1 byte out */
#define TF_DISABLE_STRETCHACK   0x10000         /* auto-disable stretch ack */
#define TF_NOBLACKHOLE_DETECTION 0x20000        /* Disable PMTU blackhole detection */
#define TF_RESCUE_RXT           0x80000         /* SACK rescue retransmit */
#define TF_CWND_NONVALIDATED    0x100000        /* cwnd non validated */
#define TF_IF_PROBING           0x200000        /* Trigger interface probe timeout */
#define TF_FASTOPEN             0x400000        /* TCP Fastopen is enabled */
#define TF_REASS_INPROG         0x800000        /* Reassembly is in progress */
#define TF_FASTOPEN_FORCE_ENABLE 0x1000000      /* Force-enable TCP Fastopen */
#define TF_USR_OUTPUT           0x2000000       /* In connect() or send() so tcp_output() can log */
#define TF_L4S_ENABLED          0x8000000       /* L4S was force enabled */
#define TF_L4S_DISABLED         0x10000000      /* L4S was force disabled */
#define TF_RACK_ENABLED         0x20000000      /* RACK is enabled */

#if TRAFFIC_MGT
	/* Inter-arrival jitter related state */
	uint32_t        iaj_rcv_ts;             /* tcp clock when the first packet was received */
	int             iaj_size;               /* Size of packet for iaj measurement */
	uint8_t         iaj_small_pkt;          /* Count of packets smaller than iaj_size */
	uint8_t         t_pipeack_ind;          /* index for next pipeack sample */
	uint16_t        iaj_pktcnt;             /* packet count, to avoid throttling initially */
	uint32_t        acc_iaj;                /* Accumulated iaj */
	uint32_t        avg_iaj;                /* Mean */
	uint32_t        std_dev_iaj;            /* Standard deviation */
#endif /* TRAFFIC_MGT */
	struct bwmeas   *t_bwmeas;              /* State for bandwidth measurement */
	tcp_seq         t_idleat;               /* rcv_nxt at idle time */
	uint8_t         t_fin_sent;
	uint8_t         t_fin_rcvd;
	uint8_t         t_rst_sent;
	uint8_t         t_rst_rcvd;
	TAILQ_ENTRY(tcpcb) t_twentry;           /* link for time wait queue */
	struct tcp_ccstate      *t_ccstate;     /* congestion control related state */
	struct tcp_ccstate      _t_ccstate;     /* congestion control related state, non-allocated */
/* Tail loss probe related state */
	tcp_seq         t_tlphighrxt;           /* snd_nxt after PTO */
	tcp_seq         t_tlphightrxt_persist;  /* like t_tlphighrxt but persists over ACKs until DSACK (if any) is processed */
	u_int32_t       t_tlpstart;             /* timestamp at PTO */
/* DSACK data receiver state */
	tcp_seq         t_dsack_lseq;           /* DSACK left sequence */
	tcp_seq         t_dsack_rseq;           /* DSACK right sequence */
/* DSACK data sender state */
	SLIST_HEAD(tcp_rxt_seghead, tcp_rxt_seg) t_rxt_segments;
	uint32_t        t_rxt_seg_count;
	uint32_t        t_rxt_seg_drop;
	tcp_seq         t_dsack_lastuna;        /* snd_una when last recovery episode started */
/* state for congestion window validation (draft-ietf-tcpm-newcwv-07) */
#define TCP_PIPEACK_SAMPLE_COUNT        3
	u_int32_t       t_pipeack_sample[TCP_PIPEACK_SAMPLE_COUNT];     /* pipeack, bytes acked within RTT */
	tcp_seq         t_pipeack_lastuna; /* una when pipeack measurement started */
	u_int32_t       t_pipeack;
	u_int32_t       t_lossflightsize;

#if MPTCP
	u_int32_t       t_mpflags;              /* flags for multipath TCP */

#define TMPF_PREESTABLISHED     0x00000001 /* conn in pre-established state */
#define TMPF_SND_KEYS           0x00000002 /* indicates that keys should be send */
#define TMPF_MPTCP_TRUE         0x00000004 /* negotiated MPTCP successfully */
/* UNUSED */
#define TMPF_SND_MPPRIO         0x00000010 /* send priority of subflow */
#define TMPF_SND_REM_ADDR       0x00000020 /* initiate address removal */
#define TMPF_RCVD_DACK          0x00000040 /* received a data-ack */
#define TMPF_JOINED_FLOW        0x00000080 /* Indicates additional flow */
#define TMPF_BACKUP_PATH        0x00000100 /* Indicates backup path */
#define TMPF_MPTCP_ACKNOW       0x00000200 /* Send Data ACK */
#define TMPF_SEND_DSN           0x00000400 /* Send DSN mapping */
#define TMPF_SEND_DFIN          0x00000800 /* Send Data FIN */
#define TMPF_RECV_DFIN          0x00001000 /* Recv Data FIN */
#define TMPF_SENT_JOIN          0x00002000 /* Sent Join */
#define TMPF_RECVD_JOIN         0x00004000 /* Received Join */
#define TMPF_RESET              0x00008000 /* Send RST */
#define TMPF_TCP_FALLBACK       0x00010000 /* Fallback to TCP */
#define TMPF_FASTCLOSERCV       0x00020000 /* Received Fastclose option */
#define TMPF_EMBED_DSN          0x00040000 /* tp has DSN mapping */
#define TMPF_MPTCP_READY        0x00080000 /* Can send DSS options on data */
#define TMPF_INFIN_SENT         0x00100000 /* Sent infinite mapping */
#define TMPF_SND_MPFAIL         0x00200000 /* Received mapping csum failure */
#define TMPF_SND_JACK           0x00400000 /* Send a Join-ACK */
#define TMPF_TFO_REQUEST        0x00800000 /* TFO Requested */
#define TMPF_MPTCP_ECHO_ADDR    0x01000000 /* MPTCP echoes add_addr */

#define TMPF_MPTCP_SIGNALS      (TMPF_SND_MPPRIO | TMPF_SND_REM_ADDR | TMPF_SND_MPFAIL | TMPF_SND_JACK | TMPF_MPTCP_ECHO_ADDR)

	tcp_seq                 t_mpuna;        /* unacknowledged sequence */
	struct mptcb            *t_mptcb;       /* pointer to MPTCP TCB */
	struct mptsub           *t_mpsub;       /* pointer to the MPTCP subflow */
	struct mpt_dsn_map      t_rcv_map;      /* Receive mapping list */
	u_int8_t                t_local_aid;    /* Addr Id for authentication */
	u_int8_t                t_rem_aid;      /* Addr ID of another subflow */
	u_int8_t                t_mprxtshift;   /* join retransmission */
#endif /* MPTCP */

#define TFO_F_OFFER_COOKIE      0x01 /* We will offer a cookie */
#define TFO_F_COOKIE_VALID      0x02 /* The received cookie is valid */
#define TFO_F_COOKIE_REQ        0x04 /* Client requested a new cookie */
#define TFO_F_COOKIE_SENT       0x08 /* Client did send a cookie in the SYN */
#define TFO_F_SYN_LOSS          0x10 /* A SYN-loss triggered a fallback to regular TCP on the client-side */
#define TFO_F_NO_SNDPROBING     0x20 /* This network is guaranteed to support TFO in the upstream direction */
#define TFO_F_HEURISTIC_DONE    0x40 /* We have already marked this network as bad */
	u_int8_t                t_tfo_flags;
#define TFO_S_SYNDATA_RCV       0x01 /* SYN+data has been received */
#define TFO_S_COOKIEREQ_RECV    0x02 /* TFO-cookie request received */
#define TFO_S_COOKIE_SENT       0x04 /* TFO-cookie announced in SYN/ACK */
#define TFO_S_COOKIE_INVALID    0x08 /* Received TFO-cookie is invalid */
#define TFO_S_COOKIE_REQ        0x10 /* TFO-cookie requested within the SYN */
#define TFO_S_COOKIE_RCV        0x20 /* TFO-cookie received in SYN/ACK */
#define TFO_S_SYN_DATA_SENT     0x40 /* SYN+data sent */
#define TFO_S_SYN_DATA_ACKED    0x80 /* SYN+data has been acknowledged in SYN/ACK */
#define TFO_S_SYN_LOSS          0x0100 /* SYN+TFO has been lost - fallback to regular TCP */
#define TFO_S_COOKIE_WRONG      0x0200 /* Cookie we sent in the SYN was wrong */
#define TFO_S_NO_COOKIE_RCV     0x0400 /* We asked for a cookie but didn't get one */
#define TFO_S_HEURISTICS_DISABLE 0x0800 /* TFO-heuristics disabled it for this connection */
#define TFO_S_SEND_BLACKHOLE    0x1000 /* TFO got blackholed in the send direction */
#define TFO_S_RECV_BLACKHOLE    0x2000 /* TFO got blackholed in the recv direction */
#define TFO_S_ONE_BYTE_PROXY    0x4000 /* TFO failed because of a proxy acknowledging just one byte */
	u_int16_t               t_tfo_stats;

	u_int8_t                t_tfo_probes; /* TFO-probes we did send */
/*
 * This here is the TFO-probing state-machine. Transitions are as follows:
 *
 * Current state: PROBE_NONE
 *		  Event: SYN+DATA acknowledged
 *			 Action: Transition to PROBE_PROBING and set keepalive-timer
 *
 * Current state: PROBE_PROBING (initial state)
 *		  Event: Receive data
 *			 Action: Transition to PROBE_NONE and cancel keepalive-timer
 *		  Event: Receive ACK that does not indicate a hole
 *			 Action: Transition to PROBE_NONE and cancel keepalive-timer
 *		  Event: Receive ACK that indicates a hole
 *			 Action: Transition to PROBE_WAIT_DATA and set a short timer
 *				 to wait for the final segment.
 *		  Event: Keepalive-timeout (did not receive any segment)
 *			 Action: Signal ETIMEDOUT as with regular keepalive-timers
 *
 * Current state: PROBE_WAIT_DATA
 *		  Event: Receive data
 *			 Action: Transition to PROBE_NONE and cancel keepalive-timer
 *		  Event: Data-timeout (did not receive the expected data)
 *			 Action: Signal ENODATA up to the app and close everything.
 */
#define TFO_PROBE_NONE          0 /* Not probing now */
#define TFO_PROBE_PROBING       1 /* Sending out TCP-keepalives waiting for reply */
#define TFO_PROBE_WAIT_DATA     2 /* Received reply, waiting for data */
	u_int8_t                t_tfo_probe_state;

	u_int32_t       t_rcvoopack;            /* out-of-order packets received */
	u_int32_t       t_pawsdrop;             /* segments dropped due to PAWS */
	u_int32_t       t_sack_recovery_episode; /* SACK recovery episodes */
	uint32_t        t_rack_recovery_episode; /* RACK recovery episodes */
	uint32_t        t_rack_reo_timeout_recovery_episode; /* RACK recovery triggered by reordering timeout */
	u_int32_t       t_reordered_pkts;       /* packets reorderd */
	u_int32_t       t_dsack_sent;           /* Sent DSACK notification */
	u_int32_t       t_dsack_recvd;          /* Received a valid DSACK option */
	SLIST_HEAD(, tcp_notify_ack_marker) t_notify_ack; /* state for notifying data acknowledgements */
	u_int32_t       t_recv_throttle_ts;     /* TS for start of recv throttle */
	u_int32_t       t_rxt_minimum_timeout;  /* minimum retransmit timeout in ms */
	uint32_t        t_challengeack_last;    /* last time challenge ACK was sent per sec */
	uint32_t        t_challengeack_count;   /* # of challenge ACKs already sent per sec */

	u_int32_t       t_connect_time;         /* time when the connection started */

	uint64_t        t_rcvwnd_limited_total_time;
	uint64_t        t_rcvwnd_limited_start_time;

	uint32_t        t_comp_gencnt; /* Current compression generation-count */
	uint32_t        t_comp_lastinc; /* Last time the gen-count was changed - should change every TCP_COMP_CHANGE_RATE ms */
#define TCP_COMP_CHANGE_RATE    5 /* Intervals at which we change the gencnt. Means that worst-case we send one ACK every TCP_COMP_CHANGE_RATE ms */

	uint32_t        t_ts_offset; /* Randomized timestamp offset to hide on-the-wire timestamp */

#define NCURR_RTT_HIST 4                         /* Number of current RTT samples (k) */
	uint32_t curr_rtt_hist[NCURR_RTT_HIST];  /* last k current RTT samples */
	uint32_t curr_rtt_min;                   /* Minimum current RTT from last k samples */
	uint32_t curr_rtt_index;                 /* Index for current RTT samples */

	tcp_seq rcv_high;                   /* highest sequence number received */
	uint32_t tsv_high;                  /* timestamp value of highest received sequence number */
	struct tcp_rledbat_state t_rlstate; /* State used by rLedbat */

	uint32_t rcv_srtt;                  /* receiver's SRTT, coarse when Timestamp not supported */
	uint32_t rcv_rtt_est_ts;            /* start of measurement for estimating RTT when Timestamp not supported */
	uint32_t rcv_rtt_est_seq;           /* expected sequence number for completion of 1RTT when Timestamp not supported */

	TAILQ_HEAD(tcp_seg_sent_head, tcp_seg_sent) t_segs_sent; /* Time ordered segment list used for RACK */
	struct tcp_seg_sent_tree_head t_segs_sent_tree; /* RB tree to track S/ACKED segments used for RACK */
	TAILQ_HEAD(tcp_seg_acked_head, tcp_seg_sent) t_segs_acked; /* Temporary storage for ACKed segments used for RACK */
	struct tcp_seg_pool seg_pool;

# define TCP_RACK_RECOVERY_PERSIST_MAX (16)
	struct tcp_rack {
		uint32_t        xmit_ts;
		uint32_t        end_seq;
		uint32_t        rtt; /* RTT of the most recently delivered segment that was not marked as invalid as a possible spurious retransmission. */
		tcp_seq         dsack_round_end;
		uint32_t        reo_wnd;
		uint8_t         reo_wnd_multi;
		uint8_t         reo_wnd_persist:5,
		    advanced:1,
		    dsack_round_seen:1,
		    segs_retransmitted:1;
	} rack;

	/*
	 * Below counters are used to estmitate current flight size,
	 * hence these counters reflect current values instead of total
	 */
	uint32_t bytes_lost;
	uint32_t bytes_retransmitted;
	uint32_t bytes_sacked;

	uuid_t          t_fsw_uuid;
	uuid_t          t_flow_uuid;
};

#define IN_FASTRECOVERY(tp)     (tp->t_flags & TF_FASTRECOVERY)
#define SACK_ENABLED(tp)        (tp->t_flagsext & TF_SACK_ENABLE)
#define TFO_ENABLED(tp)         (tp->t_flagsext & TF_FASTOPEN)
#define TCP_RACK_ENABLED(tp)    ((tp->t_flagsext & TF_RACK_ENABLED) && SACK_ENABLED(tp) && !TFO_ENABLED(tp))

/*
 * If the connection is in a throttled state due to advisory feedback from
 * the interface output queue, reset that state. We do this in favor
 * of entering recovery because the data transfer during recovery
 * should be just a trickle and it will help to improve performance.
 * We also do not want to back off twice in the same RTT.
 */
#define ENTER_FASTRECOVERY(_tp_) do {                                           \
	(_tp_)->t_flags |= TF_FASTRECOVERY;                                     \
	if (INP_IS_FLOW_CONTROLLED((_tp_)->t_inpcb))                            \
	        inp_reset_fc_state((_tp_)->t_inpcb);                            \
	if (!SLIST_EMPTY(&tp->t_rxt_segments))                                  \
	        tcp_rxtseg_clean(tp);                                           \
} while(0)

#define EXIT_FASTRECOVERY(_tp_) do {                                            \
	(_tp_)->t_flags &= ~TF_FASTRECOVERY;                                    \
	(_tp_)->t_dupacks = 0;                                                  \
	(_tp_)->t_rexmtthresh = (uint8_t)tcprexmtthresh;                        \
	(_tp_)->t_bytes_acked = 0;                                              \
	(_tp_)->ecn_flags &= ~TE_INRECOVERY;                                    \
	(_tp_)->t_timer[TCPT_PTO] = 0;                                          \
	(_tp_)->t_flagsext &= ~TF_RESCUE_RXT;                                   \
	(_tp_)->t_lossflightsize = 0;                                           \
	(_tp_)->sackhint.sack_bytes_acked = 0;                                  \
} while(0)

/*
 * When the number of duplicate acks received is less than
 * the retransmit threshold, use Limited Transmit algorithm
 */
extern uint8_t tcprexmtthresh;
#define ALLOW_LIMITED_TRANSMIT(_tp_)                                            \
	((_tp_)->t_dupacks > 0 &&                                               \
	(_tp_)->t_dupacks < (_tp_)->t_rexmtthresh &&                            \
	((_tp_)->t_flagsext & (TF_PKTS_REORDERED|TF_DELAY_RECOVERY))            \
	    != (TF_PKTS_REORDERED|TF_DELAY_RECOVERY))

/*
 * This condition is true if timestamp option is supported
 * on a connection.
 */
#define TSTMP_SUPPORTED(_tp_) \
	(((_tp_)->t_flags & (TF_REQ_TSTMP|TF_RCVD_TSTMP)) == \
	        (TF_REQ_TSTMP|TF_RCVD_TSTMP))

/*
 * This condition is true if window scale option is supported
 * on a connection
 */
#define TCP_WINDOW_SCALE_ENABLED(_tp_) \
	(((_tp_)->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) == \
	        (TF_RCVD_SCALE|TF_REQ_SCALE))

/* Is ECN negotiated end-to-end */
#define TCP_ECN_ENABLED(_tp_) \
	(((_tp_)->ecn_flags & (TE_ECN_ON)) == (TE_ECN_ON))

extern int tcp_acc_ecn;
extern int tcp_l4s;
/*
 * Accurate ECN feedback is enabled if
 * 1. It is not disabled explicitly by tcp options
 * 2. It is enabled either by sysctl or L4S toggle or A/B deployment or tcp_options,
 * It supports ACE field as well as AccECN option for ECN feedback
 */
#define TCP_ACC_ECN_ENABLED(_tp_) \
   (((_tp_)->t_flagsext & TF_L4S_DISABLED) == 0 && \
   (tcp_acc_ecn == 1 || ((_tp_)->t_flagsext & TF_L4S_ENABLED)))

/* Accurate ECN is enabled and negotiated end-to-end */
#define TCP_ACC_ECN_ON(_tp_) \
    (TCP_ACC_ECN_ENABLED(_tp_) && \
    (((_tp_)->ecn_flags & (TE_ACC_ECN_ON)) == (TE_ACC_ECN_ON)))

#define TCP_L4S_ENABLED(_tp_) \
    (((_tp_)->t_flagsext & TF_L4S_DISABLED) == 0 && \
    (tcp_l4s == 1 || ((_tp_)->t_flagsext & TF_L4S_ENABLED)))
/*
 * Gives number of bytes acked by this ack
 */
#define BYTES_ACKED(_th_, _tp_) \
	((_th_)->th_ack - (_tp_)->snd_una)

/* Returns true if a DSACK option should be sent */
#define TCP_SEND_DSACK_OPT(_tp_) \
	((_tp_)->t_dsack_lseq > 0 && (_tp_)->t_dsack_rseq > 0)

/*
 * Returns true if a DSACK sequence is within the max send window that will
 * be accepted. In order to set a window to validate sequence numbers, the
 * max send window within which a DSACK option is processed is limited.
 *
 * We need to choose a maximum window to check if the sequence number is
 * within the window. One arbitrary choice is 256 * MSS because if the
 * window is as large as 256 segments it might be big enough to ignore the
 * DSACK option. Choosing a much larger limit means that the memory for
 * retransmit segments can be held for a longer time.
 */
#define TCP_DSACK_MAX_SEND_WINDOW(_tp_) (MIN((_tp_)->snd_wnd, tcp_autosndbuf_max))
#define TCP_DSACK_SEQ_IN_WINDOW(_tp_, _seq_, _una_) \
    (SEQ_LEQ((_seq_), (_tp_)->snd_max) && \
    SEQ_GEQ((_seq_), ((_una_) - TCP_DSACK_MAX_SEND_WINDOW(_tp_))))

#define TCP_RESET_REXMT_STATE(_tp_) do { \
	(_tp_)->t_rxtshift = 0; \
	(_tp_)->t_rxtstart = 0; \
	mptcp_reset_rexmit_state((_tp_)); \
} while(0);

#define TCP_IF_STATE_CHANGED(tp, probe_if_index)                        \
	(probe_if_index > 0 && tp->t_inpcb->inp_last_outifp != NULL &&  \
	probe_if_index == tp->t_inpcb->inp_last_outifp->if_index)

#define TCP_RLEDBAT_ENABLED(_tp) \
    (tcp_rledbat == 1 && TSTMP_SUPPORTED(_tp))

#define TCP_RECV_BG(_so) \
    (tcp_recv_bg == 1 || IS_TCP_RECV_BG(_so))

#define TCP_USE_RLEDBAT(_tp, _so) \
    TCP_RLEDBAT_ENABLED(_tp) && TCP_RECV_BG(_so)

/*
 * Structure to hold TCP options that are only used during segment
 * processing (in tcp_input), but not held in the tcpcb.
 * It's basically used to reduce the number of parameters
 * to tcp_dooptions.
 */
struct tcpopt {
	uint32_t        to_flags;               /* which options are present */
#define TOF_TS          0x0001          /* timestamp */
#define TOF_MSS         0x0010
#define TOF_SCALE       0x0020
#define TOF_SIGNATURE   0x0040  /* signature option present */
#define TOF_SIGLEN      0x0080  /* signature length valid (RFC2385) */
#define TOF_SACK        0x0100          /* Peer sent SACK option */
#define TOF_MPTCP       0x0200  /* MPTCP options to be dropped */
#define TOF_TFO         0x0400  /* TFO cookie option present */
#define TOF_TFOREQ      0x0800  /* TFO cookie request present */
	uint32_t        to_tsval;
	uint32_t        to_tsecr;
	uint16_t        to_mss;
	uint8_t         to_requested_s_scale;
	uint8_t         to_nsacks;       /* number of SACK blocks */
	u_char          *to_sacks;       /* pointer to the first SACK blocks */
	u_char          *to_tfo;         /* pointer to the TFO cookie */
	uint8_t         to_num_accecn;   /* number of Accurate ECN counters */
	uint8_t         *to_accecn;      /* pointer to the first Accurate ECN counter */
	uint8_t         to_accecn_order; /* Accurate ECN ordering */
};

#define intotcpcb(ip)   ((struct tcpcb *)(ip)->inp_ppcb)
#define sototcpcb(so)   (intotcpcb(sotoinpcb(so)))

/* TFO-specific defines */
#define TFO_COOKIE_LEN_MIN      4
#define TFO_COOKIE_LEN_DEFAULT  8
#define TFO_COOKIE_LEN_MAX      16

/*
 * The initial retransmission should happen at rtt + 4 * rttvar.
 * Because of the way we do the smoothing, srtt and rttvar
 * will each average +1/2 tick of bias.  When we compute
 * the retransmit timer, we want 1/2 tick of rounding and
 * 1 extra tick because of +-1/2 tick uncertainty in the
 * firing of the timer.  The bias will give us exactly the
 * 1.5 tick we need.  But, because the bias is
 * statistical, we have to test that we don't drop below
 * the minimum feasible timer (which is 2 ticks).
 * This version of the macro adapted from a paper by Lawrence
 * Brakmo and Larry Peterson which outlines a problem caused
 * by insufficient precision in the original implementation,
 * which results in inappropriately large RTO values for very
 * fast networks.
 */
#define TCP_REXMTVAL(tp) \
	max((tp)->t_rttmin, (((tp)->t_srtt >> (TCP_RTT_SHIFT - TCP_DELTA_SHIFT))  \
	  + (tp)->t_rttvar) >> TCP_DELTA_SHIFT)

/*
 * Jaguar compatible TCP control block, for xtcpcb
 * Does not have the old fields
 */
struct otcpcb {
#else
struct tseg_qent;
_TCPCB_LIST_HEAD(tsegqe_head, tseg_qent);

struct tcpcb {
#endif /* KERNEL_PRIVATE */
#if defined(KERNEL_PRIVATE)
	u_int32_t t_segq;
#else
	struct  tsegqe_head t_segq;
#endif /* KERNEL_PRIVATE */
	int     t_dupacks;              /* consecutive dup acks recd */
	u_int32_t unused;               /* unused now: was t_template */

	int     t_timer[TCPT_NTIMERS_EXT];      /* tcp timers */

	_TCPCB_PTR(struct inpcb *) t_inpcb;     /* back pointer to internet pcb */
	int     t_state;                /* state of this connection */
	u_int   t_flags;
#define TF_ACKNOW       0x00001         /* ack peer immediately */
#define TF_DELACK       0x00002         /* ack, but try to delay it */
#define TF_NODELAY      0x00004         /* don't delay packets to coalesce */
#define TF_NOOPT        0x00008         /* don't use tcp options */
#define TF_SENTFIN      0x00010         /* have sent FIN */
#define TF_REQ_SCALE    0x00020         /* have/will request window scaling */
#define TF_RCVD_SCALE   0x00040         /* other side has requested scaling */
#define TF_REQ_TSTMP    0x00080         /* have/will request timestamps */
#define TF_RCVD_TSTMP   0x00100         /* a timestamp was received in SYN */
#define TF_SACK_PERMIT  0x00200         /* other side said I could SACK */
#define TF_NEEDSYN      0x00400         /* send SYN (implicit state) - unused but needed for backwards compatibility */
#define TF_NEEDFIN      0x00800         /* send FIN (implicit state) */
#define TF_NOPUSH       0x01000         /* don't push */
#define TF_REQ_CC       0x02000         /* have/will request CC */
#define TF_RCVD_CC      0x04000         /* a CC was received in SYN */
#define TF_SENDCCNEW    0x08000         /* Not implemented */
#define TF_MORETOCOME   0x10000         /* More data to be appended to sock */
#define TF_LQ_OVERFLOW  0x20000         /* listen queue overflow */
#define TF_RXWIN0SENT   0x40000         /* sent a receiver win 0 in response */
#define TF_SLOWLINK     0x80000         /* route is a on a modem speed link */

	int     t_force;                /* 1 if forcing out a byte */

	tcp_seq snd_una;                /* send unacknowledged */
	tcp_seq snd_max;                /* highest sequence number sent;
	                                 * used to recognize retransmits
	                                 */
	tcp_seq snd_nxt;                /* send next */
	tcp_seq snd_up;                 /* send urgent pointer */

	tcp_seq snd_wl1;                /* window update seg seq number */
	tcp_seq snd_wl2;                /* window update seg ack number */
	tcp_seq iss;                    /* initial send sequence number */
	tcp_seq irs;                    /* initial receive sequence number */

	tcp_seq rcv_nxt;                /* receive next */
	tcp_seq rcv_adv;                /* advertised window */
	u_int32_t rcv_wnd;              /* receive window */
	tcp_seq rcv_up;                 /* receive urgent pointer */

	u_int32_t snd_wnd;              /* send window */
	u_int32_t snd_cwnd;             /* congestion-controlled window */
	u_int32_t snd_ssthresh;         /* snd_cwnd size threshold for
	                                 * for slow start exponential to
	                                 * linear switch
	                                 */
	u_int   t_maxopd;               /* mss plus options */

	u_int32_t t_rcvtime;            /* time at which a packet was received */
	u_int32_t t_starttime;          /* time connection was established */
	int     t_rtttime;              /* round trip time */
	tcp_seq t_rtseq;                /* sequence number being timed */

	int     t_rxtcur;               /* current retransmit value (ticks) */
	u_int   t_maxseg;               /* maximum segment size */
	int     t_srtt;                 /* smoothed round-trip time */
	int     t_rttvar;               /* variance in round-trip time */

	int     t_rxtshift;             /* log(2) of rexmt exp. backoff */
	u_int   t_rttmin;               /* minimum rtt allowed */
	u_int32_t t_rttupdated;         /* number of times rtt sampled */
	u_int32_t max_sndwnd;           /* largest window peer has offered */

	int     t_softerror;            /* possible error not yet reported */
/* out-of-band data */
	char    t_oobflags;             /* have some */
	char    t_iobc;                 /* input character */
#define TCPOOB_HAVEDATA 0x01
#define TCPOOB_HADDATA  0x02
/* RFC 1323 variables */
	u_char  snd_scale;              /* window scaling for send window */
	u_char  rcv_scale;              /* window scaling for recv window */
	u_char  request_r_scale;        /* pending window scaling */
	u_char  requested_s_scale;
	u_int32_t ts_recent;            /* timestamp echo data */

	u_int32_t ts_recent_age;        /* when last updated */
	tcp_seq last_ack_sent;
/* RFC 1644 variables */
	tcp_cc  cc_send;                /* send connection count */
	tcp_cc  cc_recv;                /* receive connection count */
	tcp_seq snd_recover;            /* for use in fast recovery */
/* experimental */
	u_int32_t snd_cwnd_prev;        /* cwnd prior to retransmit */
	u_int32_t snd_ssthresh_prev;    /* ssthresh prior to retransmit */
	u_int32_t t_badrxtwin;          /* window for retransmit recovery */
};

#define        tcps_ecn_setup  tcps_ecn_client_success
#define        tcps_sent_cwr   tcps_ecn_recv_ece
#define        tcps_sent_ece   tcps_ecn_sent_ece

/*
 * TCP statistics.
 * Many of these should be kept per connection,
 * but that's inconvenient at the moment.
 */
struct  tcpstat {
	u_int32_t       tcps_connattempt;       /* connections initiated */
	u_int32_t       tcps_accepts;           /* connections accepted */
	u_int32_t       tcps_connects;          /* connections established */
	u_int32_t       tcps_drops;             /* connections dropped */
	u_int32_t       tcps_conndrops;         /* embryonic connections dropped */
	u_int32_t       tcps_closed;            /* conn. closed (includes drops) */
	u_int32_t       tcps_segstimed;         /* segs where we tried to get rtt */
	u_int32_t       tcps_rttupdated;        /* times we succeeded */
	u_int32_t       tcps_delack;            /* delayed acks sent */
	u_int32_t       tcps_timeoutdrop;       /* conn. dropped in rxmt timeout */
	u_int32_t       tcps_rexmttimeo;        /* retransmit timeouts */
	u_int32_t       tcps_persisttimeo;      /* persist timeouts */
	u_int32_t       tcps_keeptimeo;         /* keepalive timeouts */
	u_int32_t       tcps_keepprobe;         /* keepalive probes sent */
	u_int32_t       tcps_keepdrops;         /* connections dropped in keepalive */

	u_int32_t       tcps_sndtotal;          /* total packets sent */
	u_int32_t       tcps_sndpack;           /* data packets sent */
	u_int32_t       tcps_sndbyte;           /* data bytes sent */
	u_int32_t       tcps_sndrexmitpack;     /* data packets retransmitted */
	u_int32_t       tcps_sndrexmitbyte;     /* data bytes retransmitted */
	u_int32_t       tcps_sndacks;           /* ack-only packets sent */
	u_int32_t       tcps_sndprobe;          /* window probes sent */
	u_int32_t       tcps_sndurg;            /* packets sent with URG only */
	u_int32_t       tcps_sndwinup;          /* window update-only packets sent */
	u_int32_t       tcps_sndctrl;           /* control (SYN|FIN|RST) packets sent */

	u_int32_t       tcps_rcvtotal;          /* total packets received */
	u_int32_t       tcps_rcvpack;           /* packets received in sequence */
	u_int32_t       tcps_rcvbyte;           /* bytes received in sequence */
	u_int32_t       tcps_rcvbadsum;         /* packets received with ccksum errs */
	u_int32_t       tcps_rcvbadoff;         /* packets received with bad offset */
	u_int32_t       tcps_rcvmemdrop;        /* packets dropped for lack of memory */
	u_int32_t       tcps_rcvshort;          /* packets received too short */
	u_int32_t       tcps_rcvduppack;        /* duplicate-only packets received */
	u_int32_t       tcps_rcvdupbyte;        /* duplicate-only bytes received */
	u_int32_t       tcps_rcvpartduppack;    /* packets with some duplicate data */
	u_int32_t       tcps_rcvpartdupbyte;    /* dup. bytes in part-dup. packets */
	u_int32_t       tcps_rcvoopack;         /* out-of-order packets received */
	u_int32_t       tcps_rcvoobyte;         /* out-of-order bytes received */
	u_int32_t       tcps_rcvpackafterwin;   /* packets with data after window */
	u_int32_t       tcps_rcvbyteafterwin;   /* bytes rcvd after window */
	u_int32_t       tcps_rcvafterclose;     /* packets rcvd after "close" */
	u_int32_t       tcps_rcvwinprobe;       /* rcvd window probe packets */
	u_int32_t       tcps_rcvdupack;         /* rcvd duplicate acks */
	u_int32_t       tcps_rcvacktoomuch;     /* rcvd acks for unsent data */
	u_int32_t       tcps_rcvackpack;        /* rcvd ack packets */
	u_int32_t       tcps_rcvackbyte;        /* bytes acked by rcvd acks */
	u_int32_t       tcps_rcvwinupd;         /* rcvd window update packets */
	u_int32_t       tcps_pawsdrop;          /* segments dropped due to PAWS */
	u_int32_t       tcps_predack;           /* times hdr predict ok for acks */
	u_int32_t       tcps_preddat;           /* times hdr predict ok for data pkts */
	u_int32_t       tcps_pcbcachemiss;
	u_int32_t       tcps_cachedrtt;         /* times cached RTT in route updated */
	u_int32_t       tcps_cachedrttvar;      /* times cached rttvar updated */
	u_int32_t       tcps_cachedssthresh;    /* times cached ssthresh updated */
	u_int32_t       tcps_usedrtt;           /* times RTT initialized from route */
	u_int32_t       tcps_usedrttvar;        /* times RTTVAR initialized from rt */
	u_int32_t       tcps_usedssthresh;      /* times ssthresh initialized from rt*/
	u_int32_t       tcps_persistdrop;       /* timeout in persist state */
	u_int32_t       tcps_badsyn;            /* bogus SYN, e.g. premature ACK */
	u_int32_t       tcps_mturesent;         /* resends due to MTU discovery */
	u_int32_t       tcps_listendrop;        /* listen queue overflows */
	u_int32_t       tcps_synchallenge;      /* challenge ACK due to bad SYN */
	u_int32_t       tcps_rstchallenge;      /* challenge ACK due to bad RST */

	/* new stats from FreeBSD 5.4 sync up */
	u_int32_t       tcps_minmssdrops;       /* average minmss too low drops */

	u_int32_t       tcps_sndrexmitbad;      /* unnecessary packet retransmissions */
	u_int32_t       tcps_badrst;            /* ignored RSTs in the window */

	u_int32_t       tcps_sc_added;          /* entry added to syncache */
	u_int32_t       tcps_sc_retransmitted;  /* syncache entry was retransmitted */
	u_int32_t       tcps_sc_dupsyn;         /* duplicate SYN packet */
	u_int32_t       tcps_sc_dropped;        /* could not reply to packet */
	u_int32_t       tcps_sc_completed;      /* successful extraction of entry */
	u_int32_t       tcps_sc_bucketoverflow; /* syncache per-bucket limit hit */
	u_int32_t       tcps_sc_cacheoverflow;  /* syncache cache limit hit */
	u_int32_t       tcps_sc_reset;          /* RST removed entry from syncache */
	u_int32_t       tcps_sc_stale;          /* timed out or listen socket gone */
	u_int32_t       tcps_sc_aborted;        /* syncache entry aborted */
	u_int32_t       tcps_sc_badack;         /* removed due to bad ACK */
	u_int32_t       tcps_sc_unreach;        /* ICMP unreachable received */
	u_int32_t       tcps_sc_zonefail;       /* zalloc() failed */
	u_int32_t       tcps_sc_sendcookie;     /* SYN cookie sent */
	u_int32_t       tcps_sc_recvcookie;     /* SYN cookie received */

	u_int32_t       tcps_hc_added;          /* entry added to hostcache */
	u_int32_t       tcps_hc_bucketoverflow; /* hostcache per bucket limit hit */

	/* SACK related stats */
	u_int32_t       tcps_sack_recovery_episode; /* SACK recovery episodes */
	u_int32_t       tcps_sack_rexmits;          /* SACK rexmit segments   */
	u_int32_t       tcps_sack_rexmit_bytes;     /* SACK rexmit bytes      */
	u_int32_t       tcps_sack_rcv_blocks;       /* SACK blocks (options) received */
	u_int32_t       tcps_sack_send_blocks;      /* SACK blocks (options) sent     */
	u_int32_t       tcps_sack_sboverflow;       /* SACK sendblock overflow   */

	/* RACK related stats */
	u_int32_t       tcps_rack_recovery_episode; /* RACK recovery episodes */
	u_int32_t       tcps_rack_reordering_timeout_recovery_episode; /* RACK recovery episodes due to reordering timeout */
	u_int32_t       tcps_rack_rexmits;          /* RACK rexmit segments   */

	u_int32_t       tcps_bg_rcvtotal;       /* total background packets received */
	u_int32_t       tcps_rxtfindrop;        /* drop conn after retransmitting FIN */
	u_int32_t       tcps_fcholdpacket;      /* packets withheld because of flow control */

	u_int32_t       tcps_limited_txt;       /* Limited transmit used */
	u_int32_t       tcps_early_rexmt;       /* Early retransmit used */
	u_int32_t       tcps_sack_ackadv;       /* Cumulative ack advanced along with sack */

	/* Checksum related stats */
	u_int32_t       tcps_rcv_swcsum;        /* tcp swcksum (inbound), packets */
	u_int32_t       tcps_rcv_swcsum_bytes;  /* tcp swcksum (inbound), bytes */
	u_int32_t       tcps_rcv6_swcsum;       /* tcp6 swcksum (inbound), packets */
	u_int32_t       tcps_rcv6_swcsum_bytes; /* tcp6 swcksum (inbound), bytes */
	u_int32_t       tcps_snd_swcsum;        /* tcp swcksum (outbound), packets */
	u_int32_t       tcps_snd_swcsum_bytes;  /* tcp swcksum (outbound), bytes */
	u_int32_t       tcps_snd6_swcsum;       /* tcp6 swcksum (outbound), packets */
	u_int32_t       tcps_snd6_swcsum_bytes; /* tcp6 swcksum (outbound), bytes */
	u_int32_t       tcps_unused_1;
	u_int32_t       tcps_unused_2;
	u_int32_t       tcps_unused_3;

	/* MPTCP Related stats */
	u_int32_t       tcps_invalid_mpcap;     /* Invalid MPTCP capable opts */
	u_int32_t       tcps_invalid_joins;     /* Invalid MPTCP joins */
	u_int32_t       tcps_mpcap_fallback;    /* TCP fallback in primary */
	u_int32_t       tcps_join_fallback;     /* No MPTCP in secondary */
	u_int32_t       tcps_estab_fallback;    /* DSS option dropped */
	u_int32_t       tcps_invalid_opt;       /* Catchall error stat */
	u_int32_t       tcps_mp_outofwin;       /* Packet lies outside the
	                                         *  shared recv window */
	u_int32_t       tcps_mp_reducedwin;     /* Reduced subflow window */
	u_int32_t       tcps_mp_badcsum;        /* Bad DSS csum */
	u_int32_t       tcps_mp_oodata;         /* Out of order data */
	u_int32_t       tcps_mp_switches;       /* number of subflow switch */
	u_int32_t       tcps_mp_rcvtotal;       /* number of rcvd packets */
	u_int32_t       tcps_mp_rcvbytes;       /* number of bytes received */
	u_int32_t       tcps_mp_sndpacks;       /* number of data packs sent */
	u_int32_t       tcps_mp_sndbytes;       /* number of bytes sent */
	u_int32_t       tcps_join_rxmts;        /* join ack retransmits */
	u_int32_t       tcps_tailloss_rto;      /* RTO due to tail loss */
	u_int32_t       tcps_reordered_pkts;    /* packets reorderd */
	u_int32_t       tcps_recovered_pkts;    /* recovered after loss */
	u_int32_t       tcps_pto;               /* probe timeout */
	u_int32_t       tcps_rto_after_pto;     /* RTO after a probe */
	u_int32_t       tcps_tlp_recovery;      /* TLP induced fast recovery */
	u_int32_t       tcps_tlp_recoverlastpkt; /* TLP recoverd last pkt */
	u_int32_t       tcps_ecn_client_success; /* client-side connection negotiated ECN */
	u_int32_t       tcps_ecn_recv_ece;      /* ECE received, sent CWR */
	u_int32_t       tcps_ecn_sent_ece;      /* Sent ECE notification */
	u_int32_t       tcps_detect_reordering; /* Detect pkt reordering */
	u_int32_t       tcps_delay_recovery;    /* Delay fast recovery */
	u_int32_t       tcps_avoid_rxmt;        /* Retransmission was avoided */
	u_int32_t       tcps_unnecessary_rxmt;  /* Retransmission was not needed */
	u_int32_t       tcps_nostretchack;      /* disabled stretch ack algorithm on a connection */
	u_int32_t       tcps_rescue_rxmt;       /* SACK rescue retransmit */
	u_int32_t       tcps_pto_in_recovery;   /* rescue retransmit in fast recovery */
	u_int32_t       tcps_pmtudbh_reverted;  /* PMTU Blackhole detection, segment size reverted */

	/* DSACK related statistics */
	u_int32_t       tcps_dsack_disable;     /* DSACK disabled due to n/w duplication */
	u_int32_t       tcps_dsack_ackloss;     /* ignore DSACK due to ack loss */
	u_int32_t       tcps_dsack_badrexmt;    /* DSACK based bad rexmt recovery */
	u_int32_t       tcps_dsack_sent;        /* Sent DSACK notification */
	u_int32_t       tcps_dsack_recvd;       /* Received a valid DSACK option */
	u_int32_t       tcps_dsack_recvd_old;   /* Received an out of window DSACK option */

	/* MPTCP Subflow selection stats */
	u_int32_t       tcps_mp_sel_symtomsd;   /* By symptomsd */
	u_int32_t       tcps_mp_sel_rtt;        /* By RTT comparison */
	u_int32_t       tcps_mp_sel_rto;        /* By RTO comparison */
	u_int32_t       tcps_mp_sel_peer;       /* By peer's output pattern */
	u_int32_t       tcps_mp_num_probes;     /* Number of probes sent */
	u_int32_t       tcps_mp_verdowngrade;   /* MPTCP version downgrade */
	u_int32_t       tcps_drop_after_sleep;  /* drop after long AP sleep */
	u_int32_t       tcps_probe_if;          /* probe packets after interface availability */
	u_int32_t       tcps_probe_if_conflict; /* Can't send probe packets for interface */

	u_int32_t       tcps_ecn_client_setup;    /* Attempted ECN setup from client side */
	u_int32_t       tcps_ecn_server_setup;    /* Attempted ECN setup from server side */
	u_int32_t       tcps_ecn_server_success;  /* server-side connection negotiated ECN */
	u_int32_t       tcps_ecn_ace_syn_not_ect; /* received AccECN SYN packet with Not-ECT */
	u_int32_t       tcps_ecn_ace_syn_ect1;    /* received AccECN SYN packet with ECT1 */
	u_int32_t       tcps_ecn_ace_syn_ect0;    /* received AccECN SYN packet with ECT0 */
	u_int32_t       tcps_ecn_ace_syn_ce;      /* received AccECN SYN packet with CE */
	u_int32_t       tcps_ecn_lost_synack;   /* Lost SYN-ACK with ECN setup */
	u_int32_t       tcps_ecn_lost_syn;      /* Lost SYN with ECN setup */
	u_int32_t       tcps_ecn_not_supported; /* Server did not support ECN setup */
	u_int32_t       tcps_ecn_recv_ce;       /* Received CE from the network */
	u_int32_t       tcps_ecn_ace_recv_ce;   /* CE count received in ACE field */
	u_int32_t       tcps_ecn_conn_recv_ce;  /* Number of connections received CE atleast once */
	u_int32_t       tcps_ecn_conn_recv_ece; /* Number of connections received ECE atleast once */
	u_int32_t       tcps_ecn_conn_plnoce;   /* Number of connections that received no CE and sufferred packet loss */
	u_int32_t       tcps_ecn_conn_pl_ce;    /* Number of connections that received CE and sufferred packet loss */
	u_int32_t       tcps_ecn_conn_nopl_ce;  /* Number of connections that received CE and sufferred no packet loss */
	u_int32_t       tcps_ecn_fallback_synloss; /* Number of times we did fall back due to SYN-Loss */
	u_int32_t       tcps_ecn_fallback_reorder; /* Number of times we fallback because we detected the PAWS-issue */
	u_int32_t       tcps_ecn_fallback_ce;   /* Number of times we fallback because we received too many CEs */

	/* TFO-related statistics */
	u_int32_t       tcps_tfo_syn_data_rcv;  /* Received a SYN+data with valid cookie */
	u_int32_t       tcps_tfo_cookie_req_rcv;/* Received a TFO cookie-request */
	u_int32_t       tcps_tfo_cookie_sent;   /* Offered a TFO-cookie to the client */
	u_int32_t       tcps_tfo_cookie_invalid;/* Received an invalid TFO-cookie */
	u_int32_t       tcps_tfo_cookie_req;    /* Cookie requested with the SYN */
	u_int32_t       tcps_tfo_cookie_rcv;    /* Cookie received in a SYN/ACK */
	u_int32_t       tcps_tfo_syn_data_sent; /* SYN+data+cookie sent */
	u_int32_t       tcps_tfo_syn_data_acked;/* SYN+data has been acknowledged */
	u_int32_t       tcps_tfo_syn_loss;      /* SYN+TFO has been lost and we fallback */
	u_int32_t       tcps_tfo_blackhole;     /* TFO got blackholed by a middlebox. */
	u_int32_t       tcps_tfo_cookie_wrong;  /* TFO-cookie we sent was wrong */
	u_int32_t       tcps_tfo_no_cookie_rcv; /* We asked for a cookie but didn't get one */
	u_int32_t       tcps_tfo_heuristics_disable; /* TFO got disabled due to heuristics */
	u_int32_t       tcps_tfo_sndblackhole;  /* TFO got blackholed in the sending direction */
	u_int32_t       tcps_mss_to_default;    /* Change MSS to default using link status report */
	u_int32_t       tcps_mss_to_medium;     /* Change MSS to medium using link status report */
	u_int32_t       tcps_mss_to_low;        /* Change MSS to low using link status report */
	u_int32_t       tcps_ecn_fallback_droprst; /* ECN fallback caused by connection drop due to RST */
	u_int32_t       tcps_ecn_fallback_droprxmt; /* ECN fallback due to drop after multiple retransmits */
	u_int32_t       tcps_ecn_fallback_synrst; /* ECN fallback due to rst after syn */

	u_int32_t       tcps_mptcp_rcvmemdrop;  /* MPTCP packets dropped for lack of memory */
	u_int32_t       tcps_mptcp_rcvduppack;  /* MPTCP duplicate-only packets received */
	u_int32_t       tcps_mptcp_rcvpackafterwin; /* MPTCP packets with data after window */

	/* TCP timer statistics */
	u_int32_t       tcps_timer_drift_le_1_ms;       /* Timer drift less or equal to 1 ms */
	u_int32_t       tcps_timer_drift_le_10_ms;      /* Timer drift less or equal to 10 ms */
	u_int32_t       tcps_timer_drift_le_20_ms;      /* Timer drift less or equal to 20 ms */
	u_int32_t       tcps_timer_drift_le_50_ms;      /* Timer drift less or equal to 50 ms */
	u_int32_t       tcps_timer_drift_le_100_ms;     /* Timer drift less or equal to 100 ms */
	u_int32_t       tcps_timer_drift_le_200_ms;     /* Timer drift less or equal to 200 ms */
	u_int32_t       tcps_timer_drift_le_500_ms;     /* Timer drift less or equal to 500 ms */
	u_int32_t       tcps_timer_drift_le_1000_ms;    /* Timer drift less or equal to 1000 ms */
	u_int32_t       tcps_timer_drift_gt_1000_ms;    /* Timer drift greater than 1000 ms */

	u_int32_t       tcps_mptcp_handover_attempt;    /* Total number of MPTCP-attempts using handover mode */
	u_int32_t       tcps_mptcp_interactive_attempt; /* Total number of MPTCP-attempts using interactive mode */
	u_int32_t       tcps_mptcp_aggregate_attempt;   /* Total number of MPTCP-attempts using aggregate mode */
	u_int32_t       tcps_mptcp_fp_handover_attempt; /* Same as previous three but only for first-party apps */
	u_int32_t       tcps_mptcp_fp_interactive_attempt;
	u_int32_t       tcps_mptcp_fp_aggregate_attempt;
	u_int32_t       tcps_mptcp_heuristic_fallback;  /* Total number of MPTCP-connections that fell back due to heuristics */
	u_int32_t       tcps_mptcp_fp_heuristic_fallback;       /* Same as previous but for first-party apps */
	u_int32_t       tcps_mptcp_handover_success_wifi;       /* Total number of successfull handover-mode connections that *started* on WiFi */
	u_int32_t       tcps_mptcp_handover_success_cell;       /* Total number of successfull handover-mode connections that *started* on Cell */
	u_int32_t       tcps_mptcp_interactive_success;         /* Total number of interactive-mode connections that negotiated MPTCP */
	u_int32_t       tcps_mptcp_aggregate_success;           /* Same as previous but for aggregate */
	u_int32_t       tcps_mptcp_fp_handover_success_wifi;    /* Same as previous four, but for first-party apps */
	u_int32_t       tcps_mptcp_fp_handover_success_cell;
	u_int32_t       tcps_mptcp_fp_interactive_success;
	u_int32_t       tcps_mptcp_fp_aggregate_success;
	u_int32_t       tcps_mptcp_handover_cell_from_wifi;     /* Total number of connections that use cell in handover-mode (coming from WiFi) */
	u_int32_t       tcps_mptcp_handover_wifi_from_cell;     /* Total number of connections that use WiFi in handover-mode (coming from cell) */
	u_int32_t       tcps_mptcp_interactive_cell_from_wifi;  /* Total number of connections that use cell in interactive mode (coming from WiFi) */
	u_int64_t       tcps_mptcp_handover_cell_bytes;         /* Total number of bytes sent on cell in handover-mode (on new subflows, ignoring initial one) */
	u_int64_t       tcps_mptcp_interactive_cell_bytes;      /* Same as previous but for interactive */
	u_int64_t       tcps_mptcp_aggregate_cell_bytes;
	u_int64_t       tcps_mptcp_handover_all_bytes;          /* Total number of bytes sent in handover */
	u_int64_t       tcps_mptcp_interactive_all_bytes;
	u_int64_t       tcps_mptcp_aggregate_all_bytes;
	u_int32_t       tcps_mptcp_back_to_wifi;        /* Total number of connections that succeed to move traffic away from cell (when starting on cell) */
	u_int32_t       tcps_mptcp_wifi_proxy;          /* Total number of new subflows that fell back to regular TCP on cell */
	u_int32_t       tcps_mptcp_cell_proxy;          /* Total number of new subflows that fell back to regular TCP on WiFi */

	/* TCP offload statistics */
	u_int32_t       tcps_ka_offload_drops;  /* Keep alive drops for timeout reported by firmware */

	u_int32_t       tcps_mptcp_triggered_cell;      /* Total number of times an MPTCP-connection triggered cell bringup */

	u_int32_t       tcps_fin_timeout_drops;
};


struct tcpstat_local {
	u_int64_t badformat;
	u_int64_t unspecv6;
	u_int64_t synfin;
	u_int64_t badformatipsec;
	u_int64_t noconnnolist;
	u_int64_t noconnlist;
	u_int64_t listbadsyn;
	u_int64_t icmp6unreach;
	u_int64_t deprecate6;
	u_int64_t ooopacket;
	u_int64_t rstinsynrcv;
	u_int64_t dospacket;
	u_int64_t cleanup;
	u_int64_t synwindow;
};

#pragma pack(4)

/*
 * TCB structure exported to user-land via sysctl(3).
 * Evil hack: declare only if in_pcb.h and sys/socketvar.h have been
 * included.  Not all of our clients do.
 */

struct  xtcpcb {
	u_int32_t       xt_len;
#ifdef KERNEL_PRIVATE
	struct  inpcb_compat    xt_inp;
#else
	struct  inpcb   xt_inp;
#endif
#ifdef KERNEL_PRIVATE
	struct  otcpcb  xt_tp;
#else
	struct  tcpcb   xt_tp;
#endif
	struct  xsocket xt_socket;
	u_quad_t        xt_alignment_hack;
};

#if XNU_TARGET_OS_OSX || KERNEL || !(TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)

struct  xtcpcb64 {
	u_int32_t               xt_len;
	struct xinpcb64         xt_inpcb;

	u_int64_t t_segq;
	int     t_dupacks;              /* consecutive dup acks recd */

	int t_timer[TCPT_NTIMERS_EXT];  /* tcp timers */

	int     t_state;                /* state of this connection */
	u_int   t_flags;

	int     t_force;                /* 1 if forcing out a byte */

	tcp_seq snd_una;                /* send unacknowledged */
	tcp_seq snd_max;                /* highest sequence number sent;
	                                 * used to recognize retransmits
	                                 */
	tcp_seq snd_nxt;                /* send next */
	tcp_seq snd_up;                 /* send urgent pointer */

	tcp_seq snd_wl1;                /* window update seg seq number */
	tcp_seq snd_wl2;                /* window update seg ack number */
	tcp_seq iss;                    /* initial send sequence number */
	tcp_seq irs;                    /* initial receive sequence number */

	tcp_seq rcv_nxt;                /* receive next */
	tcp_seq rcv_adv;                /* advertised window */
	u_int32_t rcv_wnd;              /* receive window */
	tcp_seq rcv_up;                 /* receive urgent pointer */

	u_int32_t snd_wnd;              /* send window */
	u_int32_t snd_cwnd;             /* congestion-controlled window */
	u_int32_t snd_ssthresh;         /* snd_cwnd size threshold for
	                                 * for slow start exponential to
	                                 * linear switch
	                                 */
	u_int   t_maxopd;               /* mss plus options */

	u_int32_t t_rcvtime;            /* time at which a packet was received */
	u_int32_t t_starttime;          /* time connection was established */
	int     t_rtttime;              /* round trip time */
	tcp_seq t_rtseq;                /* sequence number being timed */

	int     t_rxtcur;               /* current retransmit value (ticks) */
	u_int   t_maxseg;               /* maximum segment size */
	int     t_srtt;                 /* smoothed round-trip time */
	int     t_rttvar;               /* variance in round-trip time */

	int     t_rxtshift;             /* log(2) of rexmt exp. backoff */
	u_int   t_rttmin;               /* minimum rtt allowed */
	u_int32_t t_rttupdated;         /* number of times rtt sampled */
	u_int32_t max_sndwnd;           /* largest window peer has offered */

	int     t_softerror;            /* possible error not yet reported */
/* out-of-band data */
	char    t_oobflags;             /* have some */
	char    t_iobc;                 /* input character */
/* RFC 1323 variables */
	u_char  snd_scale;              /* window scaling for send window */
	u_char  rcv_scale;              /* window scaling for recv window */
	u_char  request_r_scale;        /* pending window scaling */
	u_char  requested_s_scale;
	u_int32_t ts_recent;            /* timestamp echo data */

	u_int32_t ts_recent_age;        /* when last updated */
	tcp_seq last_ack_sent;
/* RFC 1644 variables */
	tcp_cc  cc_send;                /* send connection count */
	tcp_cc  cc_recv;                /* receive connection count */
	tcp_seq snd_recover;            /* for use in fast recovery */
/* experimental */
	u_int32_t snd_cwnd_prev;        /* cwnd prior to retransmit */
	u_int32_t snd_ssthresh_prev;    /* ssthresh prior to retransmit */
	u_int32_t t_badrxtwin;          /* window for retransmit recovery */

	u_quad_t                xt_alignment_hack;
};

#endif /* XNU_TARGET_OS_OSX || KERNEL || !(TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) */

#ifdef PRIVATE

struct  xtcpcb_n {
	u_int32_t               xt_len;
	u_int32_t                       xt_kind;                /* XSO_TCPCB */

	u_int64_t t_segq;
	int     t_dupacks;              /* consecutive dup acks recd */

	int t_timer[TCPT_NTIMERS_EXT];  /* tcp timers */

	int     t_state;                /* state of this connection */
	u_int   t_flags;

	int     t_force;                /* 1 if forcing out a byte */

	tcp_seq snd_una;                /* send unacknowledged */
	tcp_seq snd_max;                /* highest sequence number sent;
	                                 * used to recognize retransmits
	                                 */
	tcp_seq snd_nxt;                /* send next */
	tcp_seq snd_up;                 /* send urgent pointer */

	tcp_seq snd_wl1;                /* window update seg seq number */
	tcp_seq snd_wl2;                /* window update seg ack number */
	tcp_seq iss;                    /* initial send sequence number */
	tcp_seq irs;                    /* initial receive sequence number */

	tcp_seq rcv_nxt;                /* receive next */
	tcp_seq rcv_adv;                /* advertised window */
	u_int32_t rcv_wnd;              /* receive window */
	tcp_seq rcv_up;                 /* receive urgent pointer */

	u_int32_t snd_wnd;              /* send window */
	u_int32_t snd_cwnd;             /* congestion-controlled window */
	u_int32_t snd_ssthresh;         /* snd_cwnd size threshold for
	                                 * for slow start exponential to
	                                 * linear switch
	                                 */
	u_int   t_maxopd;               /* mss plus options */

	u_int32_t t_rcvtime;            /* time at which a packet was received */
	u_int32_t t_starttime;          /* time connection was established */
	int     t_rtttime;              /* round trip time */
	tcp_seq t_rtseq;                /* sequence number being timed */

	int     t_rxtcur;               /* current retransmit value (ticks) */
	u_int   t_maxseg;               /* maximum segment size */
	int     t_srtt;                 /* smoothed round-trip time */
	int     t_rttvar;               /* variance in round-trip time */

	int     t_rxtshift;             /* log(2) of rexmt exp. backoff */
	u_int   t_rttmin;               /* minimum rtt allowed */
	u_int32_t t_rttupdated;         /* number of times rtt sampled */
	u_int32_t max_sndwnd;           /* largest window peer has offered */

	int     t_softerror;            /* possible error not yet reported */
	/* out-of-band data */
	char    t_oobflags;             /* have some */
	char    t_iobc;                 /* input character */
	/* RFC 1323 variables */
	u_char  snd_scale;              /* window scaling for send window */
	u_char  rcv_scale;              /* window scaling for recv window */
	u_char  request_r_scale;        /* pending window scaling */
	u_char  requested_s_scale;
	u_int32_t ts_recent;            /* timestamp echo data */

	u_int32_t ts_recent_age;        /* when last updated */
	tcp_seq last_ack_sent;
	/* RFC 1644 variables */
	tcp_cc  cc_send;                /* send connection count */
	tcp_cc  cc_recv;                /* receive connection count */
	tcp_seq snd_recover;            /* for use in fast recovery */
	/* experimental */
	u_int32_t snd_cwnd_prev;        /* cwnd prior to retransmit */
	u_int32_t snd_ssthresh_prev;    /* ssthresh prior to retransmit */
};

/*
 * The rtt measured is in milliseconds as the timestamp granularity is
 * a millisecond. The smoothed round-trip time and estimated variance
 * are stored as fixed point numbers scaled by the values below.
 * For convenience, these scales are also used in smoothing the average
 * (smoothed = (1/scale)sample + ((scale-1)/scale)smoothed).
 * With these scales, srtt has 5 bits to the right of the binary point,
 * and thus an "ALPHA" of 0.875.  rttvar has 4 bits to the right of the
 * binary point, and is smoothed with an ALPHA of 0.75.
 */
#define TCP_RTT_SCALE           32      /* multiplier for srtt; 3 bits frac. */
#define TCP_RTT_SHIFT           5       /* shift for srtt; 5 bits frac. */
#define TCP_RTTVAR_SCALE        16      /* multiplier for rttvar; 4 bits */
#define TCP_RTTVAR_SHIFT        4       /* shift for rttvar; 4 bits */
#define TCP_DELTA_SHIFT         2       /* see tcp_input.c */

struct tcpprobereq {
	u_int64_t       ifindex;                /* Optional interface index for TCP keep-alive probing */
	u_int64_t       enable;                 /* Flag to enable or disable probing (1=on, 0=off)*/
	u_int64_t       filter_flags;           /* Optional flags for filtering interfaces per ntstat.h (NSTAT_IFNET_IS_*) */
	u_int32_t       reserved;               /* Expansion */
	u_int32_t       reserved2;              /* Expansion */
};

#endif /* PRIVATE */

#pragma pack()

/*
 * Names for TCP sysctl objects
 */
#define TCPCTL_DO_RFC1323       1       /* use RFC-1323 extensions */
#define TCPCTL_DO_RFC1644       2       /* use RFC-1644 extensions */
#define TCPCTL_MSSDFLT          3       /* MSS default */
#define TCPCTL_STATS            4       /* statistics (read-only) */
#define TCPCTL_RTTDFLT          5       /* default RTT estimate */
#define TCPCTL_KEEPIDLE         6       /* keepalive idle timer */
#define TCPCTL_KEEPINTVL        7       /* interval to send keepalives */
#define TCPCTL_SENDSPACE        8       /* send buffer space */
#define TCPCTL_RECVSPACE        9       /* receive buffer space */
#define TCPCTL_KEEPINIT         10      /* timeout for establishing syn */
#define TCPCTL_PCBLIST          11      /* list of all outstanding PCBs */
#define TCPCTL_DELACKTIME       12      /* time before sending delayed ACK */
#define TCPCTL_V6MSSDFLT        13      /* MSS default for IPv6 */
#define TCPCTL_MAXID            14

#ifdef BSD_KERNEL_PRIVATE
#include <sys/bitstring.h>

#define TCP_PKTLIST_CLEAR(tp) {                                         \
	(tp)->t_pktlist_head = (tp)->t_pktlist_tail = NULL;             \
	(tp)->t_lastchain = (tp)->t_pktlist_sentlen = 0;                \
}

extern int tcp_TCPTV_MIN;

#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_inet_tcp);
#endif /* SYSCTL_DECL */

extern  struct inpcbhead tcb;           /* head of queue of active tcpcb's */
extern  struct inpcbinfo tcbinfo;
extern  struct tcpstat tcpstat; /* tcp statistics */
extern  int tcp_mssdflt;        /* XXX */
extern  int tcp_minmss;
#define TCP_FASTOPEN_SERVER 0x01
#define TCP_FASTOPEN_CLIENT 0x02

extern int tcp_tfo_halfcnt;
extern int tcp_tfo_backlog;
extern int tcp_fastopen;
extern int ss_fltsz_local;
extern int target_qdelay;
extern uint32_t tcp_now;               /* for RFC 1323 timestamps */
extern struct timeval tcp_uptime;
extern lck_spin_t tcp_uptime_lock;
extern int tcp_delack_enabled;
extern int maxseg_unacked;
KALLOC_TYPE_DECLARE(tcp_reass_zone);
KALLOC_TYPE_DECLARE(tcp_rxt_seg_zone);
KALLOC_TYPE_DECLARE(tcp_seg_sent_zone);
extern int tcp_ecn_outbound;
extern int tcp_ecn_inbound;
extern uint32_t tcp_do_autorcvbuf;
extern uint32_t tcp_autorcvbuf_max;
extern int tcp_recv_bg;
extern int tcp_do_ack_compression;
extern int tcp_randomize_timestamps;
extern int tcp_rledbat;
extern int tcp_use_min_curr_rtt;
extern int tcp_do_timestamps;
/*
 * Dummy value used for when there is no flow and we want to ensure that compression
 * can happen.
 */
#define TCP_ACK_COMPRESSION_DUMMY 1

extern int tcp_do_better_lr;
extern int tcp_cubic_minor_fixes;
extern int tcp_cubic_rfc_compliant;
extern int tcp_flow_control_response;

extern int tcp_rack;

extern int tcp_reass_total_qlen;

struct protosw;
struct domain;

struct tcp_respond_args {
	unsigned int ifscope;
	unsigned int nocell:1,
	    noexpensive:1,
	    awdl_unrestricted:1,
	    intcoproc_allowed:1,
	    keep_alive:1,
	    noconstrained:1,
	    management_allowed:1;
};

void     tcp_canceltimers(struct tcpcb *);
struct tcpcb *
tcp_close(struct tcpcb *);
void     tcp_ctlinput(int, struct sockaddr *, void *, struct ifnet *);
int      tcp_ctloutput(struct socket *, struct sockopt *);
struct tcpcb *
tcp_drop(struct tcpcb *, int);
void     tcp_drain(void);
void     tcp_getrt_rtt(struct tcpcb *tp, struct rtentry *rt);
void     tcp_init(struct protosw *, struct domain *);
void     tcp_input(struct mbuf *, int);
void     tcp_mss(struct tcpcb *, int, unsigned int);
uint32_t tcp_ceil(double a);
uint32_t tcp_round_to(uint32_t val, uint32_t round);
uint32_t tcp_round_up(uint32_t val, uint32_t base);
uint32_t ntoh24(u_char *p);
uint32_t tcp_packets_this_ack(struct tcpcb *tp, uint32_t acked);
int      tcp_mssopt(struct tcpcb *);
void     tcp_drop_syn_sent(struct inpcb *, int);
void     tcp_mtudisc(struct inpcb *, int);
struct tcpcb *
tcp_newtcpcb(struct inpcb *);
int      tcp_output(struct tcpcb *);
void     tcp_respond(struct tcpcb *, void *, struct tcphdr *, struct mbuf *,
    tcp_seq, tcp_seq, uint8_t, struct tcp_respond_args *);
struct rtentry *
tcp_rtlookup(struct inpcb *, unsigned int);
void     tcp_setpersist(struct tcpcb *);
void     tcp_gc(struct inpcbinfo *);
void     tcp_itimer(struct inpcbinfo *ipi);
void     tcp_check_timer_state(struct tcpcb *tp);
void     tcp_run_timerlist(void *arg1, void *arg2);
void     tcp_sched_timers(struct tcpcb *tp);

struct tcptemp *tcp_maketemplate(struct tcpcb *, struct mbuf **);
void     tcp_fillheaders(struct mbuf *, struct tcpcb *, void *, void *);
struct tcpcb *tcp_timers(struct tcpcb *, int);
void     tcp_trace(int, int, struct tcpcb *, void *, struct tcphdr *, int);

void tcp_fill_info(struct tcpcb *, struct tcp_info *);
void tcp_sack_doack(struct tcpcb *, struct tcpopt *, struct tcphdr *,
    uint32_t *, uint32_t *);
extern boolean_t tcp_sack_process_dsack(struct tcpcb *, struct tcpopt *,
    struct tcphdr *, boolean_t *);
int tcp_detect_bad_rexmt(struct tcpcb *, struct tcphdr *, struct tcpopt *,
    u_int32_t rxtime);
void     tcp_update_sack_list(struct tcpcb *tp, tcp_seq rcv_laststart, tcp_seq rcv_lastend);
void     tcp_clean_sackreport(struct tcpcb *tp);
uint32_t tcp_sack_adjust(struct tcpcb *tp);
struct sackhole *tcp_sack_output(struct tcpcb *tp, int *sack_bytes_rexmt);
void     tcp_sack_partialack(struct tcpcb *, struct tcphdr *);
void     tcp_free_sackholes(struct tcpcb *tp);
int32_t  tcp_sbspace(struct tcpcb *tp);
void     tcp_set_tso(struct tcpcb *tp, struct ifnet *ifp);
void     tcp_set_ecn(struct tcpcb *tp, struct ifnet *ifp);
uint8_t  tcp_get_ace(struct tcphdr *th);
uint32_t tcp_flight_size(struct tcpcb *tp);
void     tcp_reset_stretch_ack(struct tcpcb *tp);
extern void tcp_get_ports_used(ifnet_t ifp, int, u_int32_t, bitstr_t *);
uint32_t tcp_count_opportunistic(unsigned int ifindex, u_int32_t flags);
uint32_t tcp_find_anypcb_byaddr(struct ifaddr *ifa);
void tcp_set_max_rwinscale(struct tcpcb *tp, struct socket *so);
struct bwmeas* tcp_bwmeas_alloc(struct tcpcb *tp);
void tcp_bwmeas_free(struct tcpcb *tp);
extern int32_t timer_diff(uint32_t t1, uint32_t toff1, uint32_t t2, uint32_t toff2);

/* RACK related functions */
void tcp_rack_transmit_seg(struct tcpcb *tp, struct tcp_seg_sent *seg, tcp_seq start, tcp_seq end, uint32_t xmit_ts, uint8_t flags);
void tcp_rack_update_reordering_window(struct tcpcb *tp, tcp_seq th_ack);
void tcp_rack_update_reordering_win_persist(struct tcpcb *tp);
void tcp_rack_bad_rexmt_restore(struct tcpcb *tp);
void tcp_rack_reset_segs_retransmitted(struct tcpcb *tp);
void tcp_rack_update_segment_acked(struct tcpcb *tp, uint32_t tsecr, uint32_t xmit_ts, uint32_t end_seq, bool retransmitted);
bool tcp_rack_detect_loss_and_arm_timer(struct tcpcb *tp, uint32_t dup_acks);
void tcp_rack_reordering_timeout(struct tcpcb *tp, uint32_t dup_acks);
struct tcp_seg_sent * tcp_rack_output(struct tcpcb *tp, uint32_t cwin, uint16_t *rack_seg_len);
void tcp_rack_loss_on_rto(struct tcpcb *tp, bool in_rto);
uint32_t tcp_rack_adjust(struct tcpcb *tp, uint32_t cwin);
void tcp_rack_detect_reordering_dsack(struct tcpcb *tp, tcp_seq start, tcp_seq end);
void tcp_rack_detect_reordering_acked(struct tcpcb *tp, struct tcp_seg_sent *seg);
/* RACK segment related functions */
uint32_t tcp_seg_len(struct tcp_seg_sent *seg);
void tcp_seg_sent_insert(struct tcpcb *tp, struct tcp_seg_sent *seg, tcp_seq start, tcp_seq end, uint32_t xmit_ts, uint8_t flags);
void tcp_segs_doack(struct tcpcb *tp, tcp_seq th_ack, struct tcpopt *to);
void tcp_segs_dosack(struct tcpcb *tp, tcp_seq sblk_start, tcp_seq sblk_end, uint32_t tsecr, uint32_t *newbytes_sacked);
void tcp_segs_clear_sacked(struct tcpcb *tp);
void tcp_mark_seg_lost(struct tcpcb *tp, struct tcp_seg_sent *seg);
void tcp_seg_delete(struct tcpcb *tp, struct tcp_seg_sent *seg);
void tcp_segs_sent_clean(struct tcpcb *tp, bool free_segs);

extern void tcp_set_background_cc(struct socket *);
extern void tcp_set_foreground_cc(struct socket *);
extern void tcp_set_recv_bg(struct socket *);
extern void tcp_clear_recv_bg(struct socket *);
extern boolean_t tcp_sack_byte_islost(struct tcpcb *tp);
#define IS_TCP_RECV_BG(_so)     \
	((_so)->so_flags1 & SOF1_TRAFFIC_MGT_TCP_RECVBG)

#if TRAFFIC_MGT
#define CLEAR_IAJ_STATE(_tp_) (_tp_)->iaj_rcv_ts = 0
void     reset_acc_iaj(struct tcpcb *tp);
#endif /* TRAFFIC_MGT */

int      tcp_lock(struct socket *, int, void *);
int      tcp_unlock(struct socket *, int, void *);
void     calculate_tcp_clock(void);
uint64_t microuptime_ns(void);
uint64_t tcp_pacer_get_packet_tx_time(struct tcpcb *tp, uint16_t pkt_len);
void tcp_set_mbuf_tx_time(struct mbuf *m, uint64_t tx_time);

extern void tcp_keepalive_reset(struct tcpcb *);
extern uint32_t get_base_rtt(struct tcpcb *tp);

#ifdef _KERN_LOCKS_H_
lck_mtx_t *      tcp_getlock(struct socket *, int);
#else
void *   tcp_getlock(struct socket *, int);
#endif

extern struct pr_usrreqs tcp_usrreqs;
extern uint32_t tcp_sendspace;
extern uint32_t tcp_recvspace;
extern tcp_seq tcp_new_isn(struct tcpcb *tp);

extern int tcp_input_checksum(int, struct mbuf *, struct tcphdr *, int, int);
extern void tcp_getconninfo(struct socket *, struct conninfo_tcp *);
extern void add_to_time_wait(struct tcpcb *, uint32_t delay);
extern void add_to_time_wait_now(struct tcpcb *tp, uint32_t delay);
extern void tcp_pmtud_revert_segment_size(struct tcpcb *tp);
extern void tcp_rxtseg_insert(struct tcpcb *, tcp_seq, tcp_seq);
extern struct tcp_rxt_seg *tcp_rxtseg_find(struct tcpcb *, tcp_seq, tcp_seq);
extern void tcp_rxtseg_set_spurious(struct tcpcb *tp, tcp_seq start, tcp_seq end);
extern void tcp_rxtseg_clean(struct tcpcb *);
extern boolean_t tcp_rxtseg_detect_bad_rexmt(struct tcpcb *, tcp_seq);
extern boolean_t tcp_rxtseg_dsack_for_tlp(struct tcpcb *);
extern u_int32_t tcp_rxtseg_total_size(struct tcpcb *tp);
extern void tcp_rexmt_save_state(struct tcpcb *tp);
void tcp_enter_fast_recovery(struct tcpcb *tp);
extern void tcp_interface_send_probe(u_int16_t if_index_available);
extern void tcp_probe_connectivity(struct ifnet *ifp, u_int32_t enable);
extern void tcp_get_connectivity_status(struct tcpcb *,
    struct tcp_conn_status *);

extern void tcp_clear_keep_alive_offload(struct socket *so);
extern void tcp_fill_keepalive_offload_frames(struct ifnet *,
    struct ifnet_keepalive_offload_frame *, u_int32_t, size_t, u_int32_t *);
extern int tcp_notify_kao_timeout(ifnet_t ifp,
    struct ifnet_keepalive_offload_frame *frame);

extern void tcp_disable_tfo(struct tcpcb *tp);
extern void tcp_tfo_gen_cookie(struct inpcb *inp, u_char *out, size_t blk_size);
#define TCP_FASTOPEN_KEYLEN 16
extern int tcp_freeq(struct tcpcb *tp);
extern errno_t tcp_notify_ack_id_valid(struct tcpcb *, struct socket *, u_int32_t);
extern errno_t tcp_add_notify_ack_marker(struct tcpcb *, u_int32_t);
extern void tcp_notify_ack_free(struct tcpcb *);
extern void tcp_notify_acknowledgement(struct tcpcb *, struct socket *);
extern void tcp_get_notify_ack_count(struct tcpcb *,
    struct tcp_notify_ack_complete *);
extern void tcp_get_notify_ack_ids(struct tcpcb *tp,
    struct tcp_notify_ack_complete *);
extern void tcp_update_mss_locked(struct socket *, struct ifnet *);

extern int get_tcp_inp_list(struct inpcb * __single * __counted_by(n), size_t n, inp_gen_t);
extern bool tcp_notify_ack_active(struct socket *so);
extern void tcp_set_finwait_timeout(struct tcpcb *);

#if MPTCP
extern uint16_t mptcp_output_csum(struct mbuf *m, uint64_t dss_val,
    uint32_t sseq, uint16_t dlen);
extern int mptcp_adj_mss(struct tcpcb *, boolean_t);
extern void mptcp_insert_rmap(struct tcpcb *tp, struct mbuf *m, struct tcphdr *th);
extern int dump_mptcp_reass_qlen(char *, int);
#endif

extern int dump_tcp_reass_qlen(char *, int);
extern uint32_t tcp_reass_qlen_space(struct socket *);

__private_extern__ void tcp_update_stats_per_flow(
	struct ifnet_stats_per_flow *, struct ifnet *);

#define TCP_ACK_STRATEGY_LEGACY 0
#define TCP_ACK_STRATEGY_MODERN 1

extern int tcp_ack_strategy;

#if SKYWALK
void tcp_add_fsw_flow(struct tcpcb *, struct ifnet *);
void tcp_del_fsw_flow(struct tcpcb *);
#else /* !SKYWALK */
#define tcp_add_fsw_flow(...)
#define tcp_del_fsw_flow(...)
#endif /* !SKYWALK */

#endif /* BSD_KERNEL_PRIVATE */

#endif /* _NETINET_TCP_VAR_H_ */
