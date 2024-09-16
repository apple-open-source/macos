/*
 * Copyright (c) 2011-2020 Apple Inc. All rights reserved.
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

#ifndef _NET_CLASSQ_IF_CLASSQ_H_
#define _NET_CLASSQ_IF_CLASSQ_H_

#ifdef PRIVATE
#define IFCQ_SC_MAX             10              /* max number of queues */

#ifdef BSD_KERNEL_PRIVATE
#include <net/classq/classq.h>

/* maximum number of packets stored across all queues */
#define IFCQ_DEFAULT_PKT_DROP_LIMIT     2048

/* classq request types */
typedef enum cqrq {
	CLASSQRQ_PURGE =        1,      /* purge all packets */
	CLASSQRQ_PURGE_SC =     2,      /* purge service class (and flow) */
	CLASSQRQ_EVENT =        3,      /* interface events */
	CLASSQRQ_THROTTLE =     4,      /* throttle packets */
	CLASSQRQ_STAT_SC =      5,      /* get service class queue stats */
} cqrq_t;

/* classq purge_sc request argument */
typedef struct cqrq_purge_sc {
	mbuf_svc_class_t        sc;     /* (in) service class */
	u_int32_t               flow;   /* (in) 0 means all flows */
	u_int32_t               packets; /* (out) purged packets */
	u_int32_t               bytes;  /* (out) purged bytes */
} cqrq_purge_sc_t;

/* classq throttle request argument */
typedef struct cqrq_throttle {
	u_int32_t               set;    /* set or get */
	u_int32_t               level;  /* (in/out) throttling level */
} cqrq_throttle_t;

/* classq service class stats request argument */
typedef struct cqrq_stat_sc {
	mbuf_svc_class_t        sc;     /* (in) service class */
	u_int8_t                grp_idx; /* group index */
	u_int32_t               packets; /* (out) packets enqueued */
	u_int32_t               bytes;  /* (out) bytes enqueued */
} cqrq_stat_sc_t;

/*
 * A token-bucket regulator limits the rate that a network driver can
 * dequeue packets from the output queue.  Modern cards are able to buffer
 * a large amount of packets and dequeue too many packets at a time.  This
 * bursty dequeue behavior makes it impossible to schedule packets by
 * queueing disciplines.  A token-bucket is used to control the burst size
 * in a device independent manner.
 */
struct tb_regulator {
	u_int64_t       tbr_rate_raw;   /* (unscaled) token bucket rate */
	u_int32_t       tbr_percent;    /* token bucket rate in percentage */
	int64_t         tbr_rate;       /* (scaled) token bucket rate */
	int64_t         tbr_depth;      /* (scaled) token bucket depth */

	int64_t         tbr_token;      /* (scaled) current token */
	int64_t         tbr_filluptime; /* (scaled) time to fill up bucket */
	u_int64_t       tbr_last;       /* last time token was updated */

	/*   needed for poll-and-dequeue */
};

/* simple token bucket meter profile */
struct tb_profile {
	u_int64_t       rate;   /* rate in bit-per-sec */
	u_int32_t       percent; /* rate in percentage */
	u_int32_t       depth;  /* depth in bytes */
};

struct ifclassq;
enum cqdq_op;
enum cqrq;

#if DEBUG || DEVELOPMENT
extern uint32_t ifclassq_flow_control_adv;
#endif /* DEBUG || DEVELOPMENT */
extern uint32_t ifclassq_enable_l4s;
extern unsigned int ifclassq_enable_pacing;
typedef int (*ifclassq_enq_func)(struct ifclassq *, classq_pkt_t *,
    boolean_t *);
typedef void  (*ifclassq_deq_func)(struct ifclassq *, classq_pkt_t *);
typedef void (*ifclassq_deq_sc_func)(struct ifclassq *, mbuf_svc_class_t,
    classq_pkt_t *);
typedef int (*ifclassq_deq_multi_func)(struct ifclassq *, u_int32_t,
    u_int32_t, classq_pkt_t *, classq_pkt_t *, u_int32_t *, u_int32_t *);
typedef int (*ifclassq_deq_sc_multi_func)(struct ifclassq *,
    mbuf_svc_class_t, u_int32_t, u_int32_t, classq_pkt_t *, classq_pkt_t *,
    u_int32_t *, u_int32_t *);
typedef int (*ifclassq_req_func)(struct ifclassq *, enum cqrq, void *);

/*
 * Structure defining a queue for a network interface.
 */
struct ifclassq {
	decl_lck_mtx_data(, ifcq_lock);

	os_refcnt_t     ifcq_refcnt;
	struct ifnet    *ifcq_ifp;      /* back pointer to interface */
	u_int32_t       ifcq_len;       /* packet count */
	u_int32_t       ifcq_maxlen;
	struct pktcntr  ifcq_xmitcnt;
	struct pktcntr  ifcq_dropcnt;

	u_int32_t       ifcq_type;      /* scheduler type */
	u_int32_t       ifcq_flags;     /* flags */
	u_int32_t       ifcq_sflags;    /* scheduler flags */
	u_int32_t       ifcq_target_qdelay; /* target queue delay */
	u_int32_t       ifcq_bytes;     /* bytes count */
	u_int32_t       ifcq_pkt_drop_limit;
	/* number of doorbells introduced by pacemaker thread */
	uint64_t        ifcq_doorbells;
	void            *ifcq_disc;     /* for scheduler-specific use */
	/*
	 * ifcq_disc_slots[] represents the leaf classes configured for the
	 * corresponding discpline/scheduler, ordered by their corresponding
	 * service class index.  Each slot holds the queue ID used to identify
	 * the class instance, as well as the class instance pointer itself.
	 * The latter is used during enqueue and dequeue in order to avoid the
	 * costs associated with looking up the class pointer based on the
	 * queue ID.  The queue ID is used when querying the statistics from
	 * user space.
	 *
	 * Avoiding the use of queue ID during enqueue and dequeue is made
	 * possible by virtue of knowing the particular mbuf service class
	 * associated with the packets.  The service class index of the
	 * packet is used as the index to ifcq_disc_slots[].
	 *
	 * ifcq_disc_slots[] therefore also acts as a lookup table which
	 * provides for the mapping between MBUF_SC values and the actual
	 * scheduler classes.
	 */
	struct ifclassq_disc_slot {
		u_int32_t       qid;
		void            *cl;
	} ifcq_disc_slots[IFCQ_SC_MAX]; /* for discipline use */

	/* token bucket regulator */
	struct tb_regulator     ifcq_tbr;       /* TBR */
};

/* ifcq_flags */
#define IFCQF_READY      0x01           /* ifclassq supports discipline */
#define IFCQF_ENABLED    0x02           /* ifclassq is in use */
#define IFCQF_TBR        0x04           /* Token Bucket Regulator is in use */
#define IFCQF_DESTROYED  0x08           /* ifclassq torndown */

#define IFCQ_IS_READY(_ifcq)            ((_ifcq)->ifcq_flags & IFCQF_READY)
#define IFCQ_IS_ENABLED(_ifcq)          ((_ifcq)->ifcq_flags & IFCQF_ENABLED)
#define IFCQ_TBR_IS_ENABLED(_ifcq)      ((_ifcq)->ifcq_flags & IFCQF_TBR)
#define IFCQ_IS_DESTROYED(_ifcq)        ((_ifcq)->ifcq_flags & IFCQF_DESTROYED)

/* classq enqueue return value */
/* packet has to be dropped */
#define CLASSQEQ_DROP           (-1)
/* packet successfully enqueued */
#define CLASSQEQ_SUCCESS        0
/* packet enqueued; give flow control feedback */
#define CLASSQEQ_SUCCESS_FC     1
/* packet needs to be dropped due to flowcontrol; give flow control feedback */
#define CLASSQEQ_DROP_FC        2
/* packet needs to be dropped due to suspension; give flow control feedback */
#define CLASSQEQ_DROP_SP        3
/* packet has been compressed with another one */
#define CLASSQEQ_COMPRESSED     4

/* interface event argument for CLASSQRQ_EVENT */
typedef enum cqev {
	CLASSQ_EV_INIT = 0,
	CLASSQ_EV_LINK_BANDWIDTH = 1,   /* link bandwidth has changed */
	CLASSQ_EV_LINK_LATENCY = 2,     /* link latency has changed */
	CLASSQ_EV_LINK_MTU =    3,      /* link MTU has changed */
	CLASSQ_EV_LINK_UP =     4,      /* link is now up */
	CLASSQ_EV_LINK_DOWN =   5,      /* link is now down */
} cqev_t;
#endif /* BSD_KERNEL_PRIVATE */

#define IF_CLASSQ_DEF                   0x0
#define IF_CLASSQ_LOW_LATENCY           0x1
#define IF_CLASSQ_L4S                   0x2
#define IF_DEFAULT_GRP                  0x4

#define IF_CLASSQ_ALL_GRPS              UINT8_MAX

#include <net/classq/classq.h>
#include <net/pktsched/pktsched_fq_codel.h>

#ifdef __cplusplus
extern "C" {
#endif
struct if_ifclassq_stats {
	u_int32_t       ifqs_len;
	u_int32_t       ifqs_maxlen;
	uint64_t        ifqs_doorbells;
	struct pktcntr  ifqs_xmitcnt;
	struct pktcntr  ifqs_dropcnt;
	u_int32_t       ifqs_scheduler;
	struct fq_codel_classstats      ifqs_fq_codel_stats;
} __attribute__((aligned(8)));

#ifdef __cplusplus
}
#endif

#ifdef BSD_KERNEL_PRIVATE
/*
 * For ifclassq lock
 */
#define IFCQ_LOCK_ASSERT_HELD(_ifcq)                                    \
	LCK_MTX_ASSERT(&(_ifcq)->ifcq_lock, LCK_MTX_ASSERT_OWNED)

#define IFCQ_LOCK_ASSERT_NOTHELD(_ifcq)                                 \
	LCK_MTX_ASSERT(&(_ifcq)->ifcq_lock, LCK_MTX_ASSERT_NOTOWNED)

#define IFCQ_LOCK(_ifcq)                                                \
	lck_mtx_lock(&(_ifcq)->ifcq_lock)

#define IFCQ_LOCK_SPIN(_ifcq)                                           \
	lck_mtx_lock_spin(&(_ifcq)->ifcq_lock)

#define IFCQ_CONVERT_LOCK(_ifcq) do {                                   \
	IFCQ_LOCK_ASSERT_HELD(_ifcq);                                   \
	lck_mtx_convert_spin(&(_ifcq)->ifcq_lock);                      \
} while (0)

#define IFCQ_UNLOCK(_ifcq)                                              \
	lck_mtx_unlock(&(_ifcq)->ifcq_lock)

/*
 * For ifclassq operations
 */
#define IFCQ_TBR_DEQUEUE(_ifcq, _p, _idx) do {                      \
	ifclassq_tbr_dequeue(_ifcq, _p, _idx);                          \
} while (0)

#define IFCQ_TBR_DEQUEUE_SC(_ifcq, _sc, _p, _idx) do {                        \
	ifclassq_tbr_dequeue_sc(_ifcq, _sc, _p, _idx);                        \
} while (0)

#define IFCQ_LEN(_ifcq)         ((_ifcq)->ifcq_len)
#define IFCQ_QFULL(_ifcq)       (IFCQ_LEN(_ifcq) >= (_ifcq)->ifcq_maxlen)
#define IFCQ_IS_EMPTY(_ifcq)    (IFCQ_LEN(_ifcq) == 0)
#define IFCQ_INC_LEN(_ifcq)     (IFCQ_LEN(_ifcq)++)
#define IFCQ_DEC_LEN(_ifcq)     (IFCQ_LEN(_ifcq)--)
#define IFCQ_ADD_LEN(_ifcq, _len) (IFCQ_LEN(_ifcq) += (_len))
#define IFCQ_SUB_LEN(_ifcq, _len) (IFCQ_LEN(_ifcq) -= (_len))
#define IFCQ_MAXLEN(_ifcq)      ((_ifcq)->ifcq_maxlen)
#define IFCQ_SET_MAXLEN(_ifcq, _len) ((_ifcq)->ifcq_maxlen = (_len))
#define IFCQ_TARGET_QDELAY(_ifcq)       ((_ifcq)->ifcq_target_qdelay)
#define IFCQ_BYTES(_ifcq)       ((_ifcq)->ifcq_bytes)
#define IFCQ_INC_BYTES(_ifcq, _len)     \
    ((_ifcq)->ifcq_bytes = (_ifcq)->ifcq_bytes + (_len))
#define IFCQ_DEC_BYTES(_ifcq, _len)     \
    ((_ifcq)->ifcq_bytes = (_ifcq)->ifcq_bytes - (_len))

#define IFCQ_XMIT_ADD(_ifcq, _pkt, _len) do {                           \
	PKTCNTR_ADD(&(_ifcq)->ifcq_xmitcnt, _pkt, _len);                \
} while (0)

#define IFCQ_DROP_ADD(_ifcq, _pkt, _len) do {                           \
	PKTCNTR_ADD(&(_ifcq)->ifcq_dropcnt, _pkt, _len);                \
} while (0)

#define IFCQ_PKT_DROP_LIMIT(_ifcq)      ((_ifcq)->ifcq_pkt_drop_limit)

extern int ifclassq_setup(struct ifclassq *, struct ifnet *, uint32_t);
extern void ifclassq_teardown(struct ifclassq *);
extern int ifclassq_pktsched_setup(struct ifclassq *);
extern void ifclassq_set_maxlen(struct ifclassq *, u_int32_t);
extern u_int32_t ifclassq_get_maxlen(struct ifclassq *);
extern int ifclassq_get_len(struct ifclassq *, mbuf_svc_class_t,
    u_int8_t, u_int32_t *, u_int32_t *);
extern errno_t ifclassq_enqueue(struct ifclassq *, classq_pkt_t *,
    classq_pkt_t *, u_int32_t, u_int32_t, boolean_t *);
extern errno_t ifclassq_dequeue(struct ifclassq *, u_int32_t, u_int32_t,
    classq_pkt_t *, classq_pkt_t *, u_int32_t *, u_int32_t *, u_int8_t);
extern errno_t ifclassq_dequeue_sc(struct ifclassq *, mbuf_svc_class_t,
    u_int32_t, u_int32_t, classq_pkt_t *, classq_pkt_t *, u_int32_t *,
    u_int32_t *, u_int8_t);
extern void *ifclassq_poll(struct ifclassq *, classq_pkt_type_t *);
extern void *ifclassq_poll_sc(struct ifclassq *, mbuf_svc_class_t,
    classq_pkt_type_t *);
extern void ifclassq_update(struct ifclassq *, cqev_t);
extern int ifclassq_attach(struct ifclassq *, u_int32_t, void *);
extern void ifclassq_detach(struct ifclassq *);
extern int ifclassq_getqstats(struct ifclassq *, u_int8_t, u_int32_t,
    void *, u_int32_t *);
extern const char *__null_terminated ifclassq_ev2str(cqev_t);
extern int ifclassq_tbr_set(struct ifclassq *, struct tb_profile *, boolean_t);
extern void ifclassq_tbr_dequeue(struct ifclassq *, classq_pkt_t *, u_int8_t);
extern void ifclassq_tbr_dequeue_sc(struct ifclassq *, mbuf_svc_class_t,
    classq_pkt_t *, u_int8_t);
extern void ifclassq_calc_target_qdelay(struct ifnet *ifp,
    uint64_t *if_target_qdelay, uint32_t flags);
extern void ifclassq_calc_update_interval(uint64_t *update_interval,
    uint32_t flags);
extern void ifclassq_set_packet_metadata(struct ifclassq *ifq,
    struct ifnet *ifp, classq_pkt_t *p);
extern struct ifclassq *ifclassq_alloc(void);
extern void ifclassq_retain(struct ifclassq *);
extern void ifclassq_release(struct ifclassq **);
extern int ifclassq_setup_group(struct ifclassq *ifcq, uint8_t grp_idx,
    uint8_t flags);
extern void ifclassq_set_grp_combined(struct ifclassq *ifcq, uint8_t grp_idx);
extern void ifclassq_set_grp_separated(struct ifclassq *ifcq, uint8_t grp_idx);

#endif /* BSD_KERNEL_PRIVATE */
#endif /* PRIVATE */
#endif /* _NET_CLASSQ_IF_CLASSQ_H_ */
