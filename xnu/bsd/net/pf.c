/*
 * Copyright (c) 2007-2023 Apple Inc. All rights reserved.
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

/*	$apfw: git commit 6602420f2f101b74305cd78f7cd9e0c8fdedae97 $ */
/*	$OpenBSD: pf.c,v 1.567 2008/02/20 23:40:13 henning Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2002 - 2013 Henning Brauer
 * NAT64 - Copyright (c) 2010 Viagenie Inc. (http://www.viagenie.ca)
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
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#include <machine/endian.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filio.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/mcache.h>
#include <sys/protosw.h>

#include <libkern/crypto/md5.h>
#include <libkern/libkern.h>

#include <mach/thread_act.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/route.h>
#include <net/dlil.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_fsm.h>
#include <netinet/udp_var.h>
#include <netinet/icmp_var.h>
#include <net/if_ether.h>
#include <net/ethernet.h>
#include <net/flowhash.h>
#include <net/nat464_utils.h>
#include <net/pfvar.h>
#include <net/if_pflog.h>

#if NPFSYNC
#include <net/if_pfsync.h>
#endif /* NPFSYNC */

#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>

#if DUMMYNET
#include <netinet/ip_dummynet.h>
#endif /* DUMMYNET */

#if SKYWALK
#include <skywalk/namespace/flowidns.h>
#endif /* SKYWALK */

/*
 * For RandomULong(), to get a 32 bits random value
 * Note that random() returns a 31 bits value, see rdar://11159750
 */
#include <dev/random/randomdev.h>

#define DPFPRINTF(n, x) (pf_status.debug >= (n) ? printf x : ((void)0))

/*
 * On Mac OS X, the rtableid value is treated as the interface scope
 * value that is equivalent to the interface index used for scoped
 * routing.  A valid scope value is anything but IFSCOPE_NONE (0),
 * as per definition of ifindex which is a positive, non-zero number.
 * The other BSDs treat a negative rtableid value as invalid, hence
 * the test against INT_MAX to handle userland apps which initialize
 * the field with a negative number.
 */
#define PF_RTABLEID_IS_VALID(r) \
	((r) > IFSCOPE_NONE && (r) <= INT_MAX)

/*
 * Global variables
 */
static LCK_GRP_DECLARE(pf_lock_grp, "pf");
LCK_MTX_DECLARE(pf_lock, &pf_lock_grp);

static LCK_GRP_DECLARE(pf_perim_lock_grp, "pf_perim");
LCK_RW_DECLARE(pf_perim_lock, &pf_perim_lock_grp);

/* state tables */
struct pf_state_tree_lan_ext     pf_statetbl_lan_ext;
struct pf_state_tree_ext_gwy     pf_statetbl_ext_gwy;
static uint32_t pf_state_tree_ext_gwy_nat64_cnt = 0;

struct pf_palist         pf_pabuf;
struct pf_status         pf_status;

u_int32_t                ticket_pabuf;

static MD5_CTX           pf_tcp_secret_ctx;
static u_char            pf_tcp_secret[16];
static int               pf_tcp_secret_init;
static int               pf_tcp_iss_off;

static struct pf_anchor_stackframe {
	struct pf_ruleset                       *rs;
	struct pf_rule                          *r;
	struct pf_anchor_node                   *parent;
	struct pf_anchor                        *child;
} pf_anchor_stack[64];

struct pool              pf_src_tree_pl, pf_rule_pl, pf_pooladdr_pl;
struct pool              pf_state_pl, pf_state_key_pl;

typedef void (*hook_fn_t)(void *);

struct hook_desc {
	TAILQ_ENTRY(hook_desc) hd_list;
	hook_fn_t hd_fn;
	void *hd_arg;
};

#define HOOK_REMOVE     0x01
#define HOOK_FREE       0x02
#define HOOK_ABORT      0x04

static void             *hook_establish(struct hook_desc_head *, int,
    hook_fn_t, void *);
static void             hook_runloop(struct hook_desc_head *, int flags);

struct pool              pf_app_state_pl;
static void              pf_print_addr(struct pf_addr *addr, sa_family_t af);
static void              pf_print_sk_host(struct pf_state_host *, u_int8_t, int,
    u_int8_t);

static void              pf_print_host(struct pf_addr *, u_int16_t, u_int8_t);

static void              pf_init_threshold(struct pf_threshold *, u_int32_t,
    u_int32_t);
static void              pf_add_threshold(struct pf_threshold *);
static int               pf_check_threshold(struct pf_threshold *);

static void              pf_change_ap(int, pbuf_t *, struct pf_addr *,
    u_int16_t *, u_int16_t *, u_int16_t *,
    struct pf_addr *, u_int16_t, u_int8_t, sa_family_t,
    sa_family_t, int);
static int               pf_modulate_sack(pbuf_t *, int, struct pf_pdesc *,
    struct tcphdr *, struct pf_state_peer *);
static void              pf_change_a6(struct pf_addr *, u_int16_t *,
    struct pf_addr *, u_int8_t);
static void pf_change_addr(struct pf_addr *a, u_int16_t *c, struct pf_addr *an,
    u_int8_t u, sa_family_t af, sa_family_t afn);
static void              pf_change_icmp(struct pf_addr *, u_int16_t *,
    struct pf_addr *, struct pf_addr *, u_int16_t,
    u_int16_t *, u_int16_t *, u_int16_t *,
    u_int16_t *, u_int8_t, sa_family_t);
static void              pf_send_tcp(const struct pf_rule *, sa_family_t,
    const struct pf_addr *, const struct pf_addr *,
    u_int16_t, u_int16_t, u_int32_t, u_int32_t,
    u_int8_t, u_int16_t, u_int16_t, u_int8_t, int,
    u_int16_t, struct ether_header *, struct ifnet *);
static void              pf_send_icmp(pbuf_t *, u_int8_t, u_int8_t,
    sa_family_t, struct pf_rule *);
static struct pf_rule   *pf_match_translation(struct pf_pdesc *, pbuf_t *,
    int, int, struct pfi_kif *, struct pf_addr *,
    union pf_state_xport *, struct pf_addr *,
    union pf_state_xport *, int);
static struct pf_rule   *pf_get_translation_aux(struct pf_pdesc *,
    pbuf_t *, int, int, struct pfi_kif *,
    struct pf_src_node **, struct pf_addr *,
    union pf_state_xport *, struct pf_addr *,
    union pf_state_xport *, union pf_state_xport *
#if SKYWALK
    , netns_token *
#endif
    );
static void              pf_attach_state(struct pf_state_key *,
    struct pf_state *, int);
static u_int32_t         pf_tcp_iss(struct pf_pdesc *);
static int               pf_test_rule(struct pf_rule **, struct pf_state **,
    int, struct pfi_kif *, pbuf_t *, int,
    void *, struct pf_pdesc *, struct pf_rule **,
    struct pf_ruleset **, struct ifqueue *);
#if DUMMYNET
static int               pf_test_dummynet(struct pf_rule **, int,
    struct pfi_kif *, pbuf_t **,
    struct pf_pdesc *, struct ip_fw_args *);
#endif /* DUMMYNET */
static int               pf_test_fragment(struct pf_rule **, int,
    struct pfi_kif *, pbuf_t *, void *,
    struct pf_pdesc *, struct pf_rule **,
    struct pf_ruleset **);
static int               pf_test_state_tcp(struct pf_state **, int,
    struct pfi_kif *, pbuf_t *, int,
    void *, struct pf_pdesc *, u_short *);
static int               pf_test_state_udp(struct pf_state **, int,
    struct pfi_kif *, pbuf_t *, int,
    void *, struct pf_pdesc *, u_short *);
static int               pf_test_state_icmp(struct pf_state **, int,
    struct pfi_kif *, pbuf_t *, int,
    void *, struct pf_pdesc *, u_short *);
static int               pf_test_state_other(struct pf_state **, int,
    struct pfi_kif *, struct pf_pdesc *);
static int               pf_match_tag(struct pf_rule *,
    struct pf_mtag *, int *);
static void              pf_hash(struct pf_addr *, struct pf_addr *,
    struct pf_poolhashkey *, sa_family_t);
static int               pf_map_addr(u_int8_t, struct pf_rule *,
    struct pf_addr *, struct pf_addr *,
    struct pf_addr *, struct pf_src_node **);
static int               pf_get_sport(struct pf_pdesc *, struct pfi_kif *,
    struct pf_rule *, struct pf_addr *,
    union pf_state_xport *, struct pf_addr *,
    union pf_state_xport *, struct pf_addr *,
    union pf_state_xport *, struct pf_src_node **
#if SKYWALK
    , netns_token *
#endif
    );
static void              pf_route(pbuf_t **, struct pf_rule *, int,
    struct ifnet *, struct pf_state *,
    struct pf_pdesc *);
static void              pf_route6(pbuf_t **, struct pf_rule *, int,
    struct ifnet *, struct pf_state *,
    struct pf_pdesc *);
static u_int8_t          pf_get_wscale(pbuf_t *, int, u_int16_t,
    sa_family_t);
static u_int16_t         pf_get_mss(pbuf_t *, int, u_int16_t,
    sa_family_t);
static u_int16_t         pf_calc_mss(struct pf_addr *, sa_family_t,
    u_int16_t);
static void              pf_set_rt_ifp(struct pf_state *,
    struct pf_addr *, sa_family_t af);
static int               pf_check_proto_cksum(pbuf_t *, int, int,
    u_int8_t, sa_family_t);
static int               pf_addr_wrap_neq(struct pf_addr_wrap *,
    struct pf_addr_wrap *);
static struct pf_state  *pf_find_state(struct pfi_kif *,
    struct pf_state_key_cmp *, u_int);
static int               pf_src_connlimit(struct pf_state **);
static void              pf_stateins_err(const char *, struct pf_state *,
    struct pfi_kif *);
static int               pf_check_congestion(struct ifqueue *);

#if 0
static const char *pf_pptp_ctrl_type_name(u_int16_t code);
#endif
static void             pf_pptp_handler(struct pf_state *, int, int,
    struct pf_pdesc *, struct pfi_kif *);
static void             pf_pptp_unlink(struct pf_state *);
static void             pf_grev1_unlink(struct pf_state *);
static int              pf_test_state_grev1(struct pf_state **, int,
    struct pfi_kif *, int, struct pf_pdesc *);
static int              pf_ike_compare(struct pf_app_state *,
    struct pf_app_state *);
static int              pf_test_state_esp(struct pf_state **, int,
    struct pfi_kif *, int, struct pf_pdesc *);
static int pf_test6(int, struct ifnet *, pbuf_t **, struct ether_header *,
    struct ip_fw_args *);
#if INET
static int pf_test(int, struct ifnet *, pbuf_t **,
    struct ether_header *, struct ip_fw_args *);
#endif /* INET */


extern struct pool pfr_ktable_pl;
extern struct pool pfr_kentry_pl;
extern int path_mtu_discovery;

struct pf_pool_limit pf_pool_limits[PF_LIMIT_MAX] = {
	{ .pp = &pf_state_pl, .limit = PFSTATE_HIWAT },
	{ .pp = &pf_app_state_pl, .limit = PFAPPSTATE_HIWAT },
	{ .pp = &pf_src_tree_pl, .limit = PFSNODE_HIWAT },
	{ .pp = &pf_frent_pl, .limit = PFFRAG_FRENT_HIWAT },
	{ .pp = &pfr_ktable_pl, .limit = PFR_KTABLE_HIWAT },
	{ .pp = &pfr_kentry_pl, .limit = PFR_KENTRY_HIWAT },
};

#if SKYWALK
const char *compatible_anchors[] = {
	"com.apple.internet-sharing",
	"com.apple/250.ApplicationFirewall",
	"com.apple/200.AirDrop"
};
#endif // SKYWALK

void *
pf_lazy_makewritable(struct pf_pdesc *pd, pbuf_t *pbuf, int len)
{
	void *__single p;

	if (pd->lmw < 0) {
		return NULL;
	}

	VERIFY(pbuf == pd->mp);

	p = pbuf->pb_data;
	if (len > pd->lmw) {
		if ((p = pbuf_ensure_writable(pbuf, len)) == NULL) {
			len = -1;
		}
		pd->lmw = len;
		if (len >= 0) {
			pd->pf_mtag = pf_find_mtag_pbuf(pbuf);

			switch (pd->af) {
			case AF_INET: {
				struct ip *__single h = p;
				pd->src = (struct pf_addr *)(void *)&h->ip_src;
				pd->dst = (struct pf_addr *)(void *)&h->ip_dst;
				pd->ip_sum = &h->ip_sum;
				break;
			}
			case AF_INET6: {
				struct ip6_hdr *__single h = p;
				pd->src = (struct pf_addr *)(void *)&h->ip6_src;
				pd->dst = (struct pf_addr *)(void *)&h->ip6_dst;
				break;
			}
			}
		}
	}

	return len < 0 ? NULL : p;
}

static const int *
pf_state_lookup_aux(struct pf_state **state, struct pfi_kif *kif,
    int direction, int *action)
{
	if (*state == NULL || (*state)->timeout == PFTM_PURGE) {
		*action = PF_DROP;
		return action;
	}

	if (direction == PF_OUT &&
	    (((*state)->rule.ptr->rt == PF_ROUTETO &&
	    (*state)->rule.ptr->direction == PF_OUT) ||
	    ((*state)->rule.ptr->rt == PF_REPLYTO &&
	    (*state)->rule.ptr->direction == PF_IN)) &&
	    (*state)->rt_kif != NULL && (*state)->rt_kif != kif) {
		*action = PF_PASS;
		return action;
	}

	return 0;
}

#define STATE_LOOKUP()                                                   \
	do {                                                             \
	        int action;                                              \
	        *state = pf_find_state(kif, &key, direction);            \
	        if (*state != NULL && pd != NULL &&                      \
	            !(pd->pktflags & PKTF_FLOW_ID)) {                    \
	                pd->flowsrc = (*state)->state_key->flowsrc;      \
	                pd->flowhash = (*state)->state_key->flowhash;    \
	                if (pd->flowhash != 0) {                         \
	                        pd->pktflags |= PKTF_FLOW_ID;            \
	                        pd->pktflags &= ~PKTF_FLOW_ADV;          \
	                }                                                \
	        }                                                        \
	        if (pf_state_lookup_aux(state, kif, direction, &action)) \
	                return (action);                                 \
	} while (0)

/*
 * This macro resets the flowID information in a packet descriptor which was
 * copied in from a PF state. This should be used after a protocol state lookup
 * finds a matching PF state, but then decides to not use it for various
 * reasons.
 */
#define PD_CLEAR_STATE_FLOWID(_pd)                                       \
	do {                                                             \
	        if (__improbable(((_pd)->pktflags & PKTF_FLOW_ID) &&     \
	            ((_pd)->flowsrc == FLOWSRC_PF))) {                   \
	                (_pd)->flowhash = 0;                             \
	                (_pd)->flowsrc = 0;                              \
	                (_pd)->pktflags &= ~PKTF_FLOW_ID;                \
	        }                                                        \
                                                                         \
	} while (0)

#define STATE_ADDR_TRANSLATE(sk)                                        \
	(sk)->lan.addr.addr32[0] != (sk)->gwy.addr.addr32[0] ||         \
	((sk)->af_lan == AF_INET6 &&                                    \
	((sk)->lan.addr.addr32[1] != (sk)->gwy.addr.addr32[1] ||        \
	(sk)->lan.addr.addr32[2] != (sk)->gwy.addr.addr32[2] ||         \
	(sk)->lan.addr.addr32[3] != (sk)->gwy.addr.addr32[3]))

#define STATE_TRANSLATE(sk)                                             \
	((sk)->af_lan != (sk)->af_gwy ||                                \
	STATE_ADDR_TRANSLATE(sk) ||                                     \
	(sk)->lan.xport.port != (sk)->gwy.xport.port)

#define STATE_GRE_TRANSLATE(sk)                                         \
	(STATE_ADDR_TRANSLATE(sk) ||                                    \
	(sk)->lan.xport.call_id != (sk)->gwy.xport.call_id)

#define BOUND_IFACE(r, k) \
	((r)->rule_flag & PFRULE_IFBOUND) ? (k) : pfi_all

#define STATE_INC_COUNTERS(s)                                   \
	do {                                                    \
	        s->rule.ptr->states++;                          \
	        VERIFY(s->rule.ptr->states != 0);               \
	        if (s->anchor.ptr != NULL) {                    \
	                s->anchor.ptr->states++;                \
	                VERIFY(s->anchor.ptr->states != 0);     \
	        }                                               \
	        if (s->nat_rule.ptr != NULL) {                  \
	                s->nat_rule.ptr->states++;              \
	                VERIFY(s->nat_rule.ptr->states != 0);   \
	        }                                               \
	} while (0)

#define STATE_DEC_COUNTERS(s)                                   \
	do {                                                    \
	        if (s->nat_rule.ptr != NULL) {                  \
	                VERIFY(s->nat_rule.ptr->states > 0);    \
	                s->nat_rule.ptr->states--;              \
	        }                                               \
	        if (s->anchor.ptr != NULL) {                    \
	                VERIFY(s->anchor.ptr->states > 0);      \
	                s->anchor.ptr->states--;                \
	        }                                               \
	        VERIFY(s->rule.ptr->states > 0);                \
	        s->rule.ptr->states--;                          \
	} while (0)

static __inline int pf_src_compare(struct pf_src_node *, struct pf_src_node *);
static __inline int pf_state_compare_lan_ext(struct pf_state_key *,
    struct pf_state_key *);
static __inline int pf_state_compare_ext_gwy(struct pf_state_key *,
    struct pf_state_key *);
static __inline int pf_state_compare_id(struct pf_state *,
    struct pf_state *);

struct pf_src_tree tree_src_tracking;

struct pf_state_tree_id tree_id;
struct pf_state_queue state_list;

RB_GENERATE(pf_src_tree, pf_src_node, entry, pf_src_compare);
RB_GENERATE(pf_state_tree_lan_ext, pf_state_key,
    entry_lan_ext, pf_state_compare_lan_ext);
RB_GENERATE(pf_state_tree_ext_gwy, pf_state_key,
    entry_ext_gwy, pf_state_compare_ext_gwy);
RB_GENERATE(pf_state_tree_id, pf_state,
    entry_id, pf_state_compare_id);

#define PF_DT_SKIP_LANEXT       0x01
#define PF_DT_SKIP_EXTGWY       0x02

static const u_int16_t PF_PPTP_PORT = 1723;
static const u_int32_t PF_PPTP_MAGIC_NUMBER = 0x1A2B3C4D;

struct pf_pptp_hdr {
	u_int16_t       length;
	u_int16_t       type;
	u_int32_t       magic;
};

struct pf_pptp_ctrl_hdr {
	u_int16_t       type;
	u_int16_t       reserved_0;
};

struct pf_pptp_ctrl_generic {
	u_int16_t       data[0];
};

#define PF_PPTP_CTRL_TYPE_START_REQ     1
struct pf_pptp_ctrl_start_req {
	u_int16_t       protocol_version;
	u_int16_t       reserved_1;
	u_int32_t       framing_capabilities;
	u_int32_t       bearer_capabilities;
	u_int16_t       maximum_channels;
	u_int16_t       firmware_revision;
	u_int8_t        host_name[64];
	u_int8_t        vendor_string[64];
};

#define PF_PPTP_CTRL_TYPE_START_RPY     2
struct pf_pptp_ctrl_start_rpy {
	u_int16_t       protocol_version;
	u_int8_t        result_code;
	u_int8_t        error_code;
	u_int32_t       framing_capabilities;
	u_int32_t       bearer_capabilities;
	u_int16_t       maximum_channels;
	u_int16_t       firmware_revision;
	u_int8_t        host_name[64];
	u_int8_t        vendor_string[64];
};

#define PF_PPTP_CTRL_TYPE_STOP_REQ      3
struct pf_pptp_ctrl_stop_req {
	u_int8_t        reason;
	u_int8_t        reserved_1;
	u_int16_t       reserved_2;
};

#define PF_PPTP_CTRL_TYPE_STOP_RPY      4
struct pf_pptp_ctrl_stop_rpy {
	u_int8_t        reason;
	u_int8_t        error_code;
	u_int16_t       reserved_1;
};

#define PF_PPTP_CTRL_TYPE_ECHO_REQ      5
struct pf_pptp_ctrl_echo_req {
	u_int32_t       identifier;
};

#define PF_PPTP_CTRL_TYPE_ECHO_RPY      6
struct pf_pptp_ctrl_echo_rpy {
	u_int32_t       identifier;
	u_int8_t        result_code;
	u_int8_t        error_code;
	u_int16_t       reserved_1;
};

#define PF_PPTP_CTRL_TYPE_CALL_OUT_REQ  7
struct pf_pptp_ctrl_call_out_req {
	u_int16_t       call_id;
	u_int16_t       call_sernum;
	u_int32_t       min_bps;
	u_int32_t       bearer_type;
	u_int32_t       framing_type;
	u_int16_t       rxwindow_size;
	u_int16_t       proc_delay;
	u_int8_t        phone_num[64];
	u_int8_t        sub_addr[64];
};

#define PF_PPTP_CTRL_TYPE_CALL_OUT_RPY  8
struct pf_pptp_ctrl_call_out_rpy {
	u_int16_t       call_id;
	u_int16_t       peer_call_id;
	u_int8_t        result_code;
	u_int8_t        error_code;
	u_int16_t       cause_code;
	u_int32_t       connect_speed;
	u_int16_t       rxwindow_size;
	u_int16_t       proc_delay;
	u_int32_t       phy_channel_id;
};

#define PF_PPTP_CTRL_TYPE_CALL_IN_1ST   9
struct pf_pptp_ctrl_call_in_1st {
	u_int16_t       call_id;
	u_int16_t       call_sernum;
	u_int32_t       bearer_type;
	u_int32_t       phy_channel_id;
	u_int16_t       dialed_number_len;
	u_int16_t       dialing_number_len;
	u_int8_t        dialed_num[64];
	u_int8_t        dialing_num[64];
	u_int8_t        sub_addr[64];
};

#define PF_PPTP_CTRL_TYPE_CALL_IN_2ND   10
struct pf_pptp_ctrl_call_in_2nd {
	u_int16_t       call_id;
	u_int16_t       peer_call_id;
	u_int8_t        result_code;
	u_int8_t        error_code;
	u_int16_t       rxwindow_size;
	u_int16_t       txdelay;
	u_int16_t       reserved_1;
};

#define PF_PPTP_CTRL_TYPE_CALL_IN_3RD   11
struct pf_pptp_ctrl_call_in_3rd {
	u_int16_t       call_id;
	u_int16_t       reserved_1;
	u_int32_t       connect_speed;
	u_int16_t       rxwindow_size;
	u_int16_t       txdelay;
	u_int32_t       framing_type;
};

#define PF_PPTP_CTRL_TYPE_CALL_CLR      12
struct pf_pptp_ctrl_call_clr {
	u_int16_t       call_id;
	u_int16_t       reserved_1;
};

#define PF_PPTP_CTRL_TYPE_CALL_DISC     13
struct pf_pptp_ctrl_call_disc {
	u_int16_t       call_id;
	u_int8_t        result_code;
	u_int8_t        error_code;
	u_int16_t       cause_code;
	u_int16_t       reserved_1;
	u_int8_t        statistics[128];
};

#define PF_PPTP_CTRL_TYPE_ERROR 14
struct pf_pptp_ctrl_error {
	u_int16_t       peer_call_id;
	u_int16_t       reserved_1;
	u_int32_t       crc_errors;
	u_int32_t       fr_errors;
	u_int32_t       hw_errors;
	u_int32_t       buf_errors;
	u_int32_t       tim_errors;
	u_int32_t       align_errors;
};

#define PF_PPTP_CTRL_TYPE_SET_LINKINFO  15
struct pf_pptp_ctrl_set_linkinfo {
	u_int16_t       peer_call_id;
	u_int16_t       reserved_1;
	u_int32_t       tx_accm;
	u_int32_t       rx_accm;
};

static const size_t PF_PPTP_CTRL_MSG_MINSIZE =
    sizeof(struct pf_pptp_hdr) + sizeof(struct pf_pptp_ctrl_hdr);

union pf_pptp_ctrl_msg_union {
	struct pf_pptp_ctrl_start_req           start_req;
	struct pf_pptp_ctrl_start_rpy           start_rpy;
	struct pf_pptp_ctrl_stop_req            stop_req;
	struct pf_pptp_ctrl_stop_rpy            stop_rpy;
	struct pf_pptp_ctrl_echo_req            echo_req;
	struct pf_pptp_ctrl_echo_rpy            echo_rpy;
	struct pf_pptp_ctrl_call_out_req        call_out_req;
	struct pf_pptp_ctrl_call_out_rpy        call_out_rpy;
	struct pf_pptp_ctrl_call_in_1st         call_in_1st;
	struct pf_pptp_ctrl_call_in_2nd         call_in_2nd;
	struct pf_pptp_ctrl_call_in_3rd         call_in_3rd;
	struct pf_pptp_ctrl_call_clr            call_clr;
	struct pf_pptp_ctrl_call_disc           call_disc;
	struct pf_pptp_ctrl_error                       error;
	struct pf_pptp_ctrl_set_linkinfo        set_linkinfo;
	u_int8_t                                                        data[0];
};

struct pf_pptp_ctrl_msg {
	struct pf_pptp_hdr                              hdr;
	struct pf_pptp_ctrl_hdr                 ctrl;
	union pf_pptp_ctrl_msg_union    msg;
};

#define PF_GRE_FLAG_CHECKSUM_PRESENT    0x8000
#define PF_GRE_FLAG_VERSION_MASK                0x0007
#define PF_GRE_PPP_ETHERTYPE                    0x880B

static const u_int16_t PF_IKE_PORT = 500;

struct pf_ike_hdr {
	u_int64_t initiator_cookie, responder_cookie;
	u_int8_t next_payload, version, exchange_type, flags;
	u_int32_t message_id, length;
};

#define PF_IKE_PACKET_MINSIZE   (sizeof (struct pf_ike_hdr))

#define PF_IKEv1_EXCHTYPE_BASE                           1
#define PF_IKEv1_EXCHTYPE_ID_PROTECT             2
#define PF_IKEv1_EXCHTYPE_AUTH_ONLY                      3
#define PF_IKEv1_EXCHTYPE_AGGRESSIVE             4
#define PF_IKEv1_EXCHTYPE_INFORMATIONAL          5
#define PF_IKEv2_EXCHTYPE_SA_INIT                       34
#define PF_IKEv2_EXCHTYPE_AUTH                          35
#define PF_IKEv2_EXCHTYPE_CREATE_CHILD_SA       36
#define PF_IKEv2_EXCHTYPE_INFORMATIONAL         37

#define PF_IKEv1_FLAG_E         0x01
#define PF_IKEv1_FLAG_C         0x02
#define PF_IKEv1_FLAG_A         0x04
#define PF_IKEv2_FLAG_I         0x08
#define PF_IKEv2_FLAG_V         0x10
#define PF_IKEv2_FLAG_R         0x20


static __inline int
pf_addr_compare(struct pf_addr *a, struct pf_addr *b, sa_family_t af)
{
	switch (af) {
#ifdef INET
	case AF_INET:
		if (a->addr32[0] > b->addr32[0]) {
			return 1;
		}
		if (a->addr32[0] < b->addr32[0]) {
			return -1;
		}
		break;
#endif /* INET */
	case AF_INET6:
		if (a->addr32[3] > b->addr32[3]) {
			return 1;
		}
		if (a->addr32[3] < b->addr32[3]) {
			return -1;
		}
		if (a->addr32[2] > b->addr32[2]) {
			return 1;
		}
		if (a->addr32[2] < b->addr32[2]) {
			return -1;
		}
		if (a->addr32[1] > b->addr32[1]) {
			return 1;
		}
		if (a->addr32[1] < b->addr32[1]) {
			return -1;
		}
		if (a->addr32[0] > b->addr32[0]) {
			return 1;
		}
		if (a->addr32[0] < b->addr32[0]) {
			return -1;
		}
		break;
	}
	return 0;
}

static __inline int
pf_src_compare(struct pf_src_node *a, struct pf_src_node *b)
{
	int     diff;

	if (a->rule.ptr > b->rule.ptr) {
		return 1;
	}
	if (a->rule.ptr < b->rule.ptr) {
		return -1;
	}
	if ((diff = a->af - b->af) != 0) {
		return diff;
	}
	if ((diff = pf_addr_compare(&a->addr, &b->addr, a->af)) != 0) {
		return diff;
	}
	return 0;
}

static __inline int
pf_state_compare_lan_ext(struct pf_state_key *a, struct pf_state_key *b)
{
	int     diff;
	int     extfilter;

	if ((diff = a->proto - b->proto) != 0) {
		return diff;
	}
	if ((diff = a->af_lan - b->af_lan) != 0) {
		return diff;
	}

	extfilter = PF_EXTFILTER_APD;

	switch (a->proto) {
	case IPPROTO_ICMP:
	case IPPROTO_ICMPV6:
		if ((diff = a->lan.xport.port - b->lan.xport.port) != 0) {
			return diff;
		}
		break;

	case IPPROTO_TCP:
		if ((diff = a->lan.xport.port - b->lan.xport.port) != 0) {
			return diff;
		}
		if ((diff = a->ext_lan.xport.port - b->ext_lan.xport.port) != 0) {
			return diff;
		}
		break;

	case IPPROTO_UDP:
		if ((diff = a->proto_variant - b->proto_variant)) {
			return diff;
		}
		extfilter = a->proto_variant;
		if ((diff = a->lan.xport.port - b->lan.xport.port) != 0) {
			return diff;
		}
		if ((extfilter < PF_EXTFILTER_AD) &&
		    (diff = a->ext_lan.xport.port - b->ext_lan.xport.port) != 0) {
			return diff;
		}
		break;

	case IPPROTO_GRE:
		if (a->proto_variant == PF_GRE_PPTP_VARIANT &&
		    a->proto_variant == b->proto_variant) {
			if (!!(diff = a->ext_lan.xport.call_id -
			    b->ext_lan.xport.call_id)) {
				return diff;
			}
		}
		break;

	case IPPROTO_ESP:
		if (!!(diff = a->ext_lan.xport.spi - b->ext_lan.xport.spi)) {
			return diff;
		}
		break;

	default:
		break;
	}

	switch (a->af_lan) {
#if INET
	case AF_INET:
		if ((diff = pf_addr_compare(&a->lan.addr, &b->lan.addr,
		    a->af_lan)) != 0) {
			return diff;
		}

		if (extfilter < PF_EXTFILTER_EI) {
			if ((diff = pf_addr_compare(&a->ext_lan.addr,
			    &b->ext_lan.addr,
			    a->af_lan)) != 0) {
				return diff;
			}
		}
		break;
#endif /* INET */
	case AF_INET6:
		if ((diff = pf_addr_compare(&a->lan.addr, &b->lan.addr,
		    a->af_lan)) != 0) {
			return diff;
		}

		if (extfilter < PF_EXTFILTER_EI ||
		    !PF_AZERO(&b->ext_lan.addr, AF_INET6)) {
			if ((diff = pf_addr_compare(&a->ext_lan.addr,
			    &b->ext_lan.addr,
			    a->af_lan)) != 0) {
				return diff;
			}
		}
		break;
	}

	if (a->app_state && b->app_state) {
		if (a->app_state->compare_lan_ext &&
		    b->app_state->compare_lan_ext) {
			diff = (const char *)b->app_state->compare_lan_ext -
			    (const char *)a->app_state->compare_lan_ext;
			if (diff != 0) {
				return diff;
			}
			diff = a->app_state->compare_lan_ext(a->app_state,
			    b->app_state);
			if (diff != 0) {
				return diff;
			}
		}
	}

	return 0;
}

static __inline int
pf_state_compare_ext_gwy(struct pf_state_key *a, struct pf_state_key *b)
{
	int     diff;
	int     extfilter;
	int     a_nat64, b_nat64;

	if ((diff = a->proto - b->proto) != 0) {
		return diff;
	}

	if ((diff = a->af_gwy - b->af_gwy) != 0) {
		return diff;
	}

	a_nat64 = (a->af_lan == PF_INET6 && a->af_gwy == PF_INET) ? 1 : 0;
	b_nat64 = (b->af_lan == PF_INET6 && b->af_gwy == PF_INET) ? 1 : 0;
	if ((diff = a_nat64 - b_nat64) != 0) {
		return diff;
	}

	extfilter = PF_EXTFILTER_APD;

	switch (a->proto) {
	case IPPROTO_ICMP:
	case IPPROTO_ICMPV6:
		if ((diff = a->gwy.xport.port - b->gwy.xport.port) != 0) {
			return diff;
		}
		break;

	case IPPROTO_TCP:
		if ((diff = a->ext_gwy.xport.port - b->ext_gwy.xport.port) != 0) {
			return diff;
		}
		if ((diff = a->gwy.xport.port - b->gwy.xport.port) != 0) {
			return diff;
		}
		break;

	case IPPROTO_UDP:
		if ((diff = a->proto_variant - b->proto_variant)) {
			return diff;
		}
		extfilter = a->proto_variant;
		if ((diff = a->gwy.xport.port - b->gwy.xport.port) != 0) {
			return diff;
		}
		if ((extfilter < PF_EXTFILTER_AD) &&
		    (diff = a->ext_gwy.xport.port - b->ext_gwy.xport.port) != 0) {
			return diff;
		}
		break;

	case IPPROTO_GRE:
		if (a->proto_variant == PF_GRE_PPTP_VARIANT &&
		    a->proto_variant == b->proto_variant) {
			if (!!(diff = a->gwy.xport.call_id -
			    b->gwy.xport.call_id)) {
				return diff;
			}
		}
		break;

	case IPPROTO_ESP:
		if (!!(diff = a->gwy.xport.spi - b->gwy.xport.spi)) {
			return diff;
		}
		break;

	default:
		break;
	}

	switch (a->af_gwy) {
#if INET
	case AF_INET:
		if ((diff = pf_addr_compare(&a->gwy.addr, &b->gwy.addr,
		    a->af_gwy)) != 0) {
			return diff;
		}

		if (extfilter < PF_EXTFILTER_EI) {
			if ((diff = pf_addr_compare(&a->ext_gwy.addr, &b->ext_gwy.addr,
			    a->af_gwy)) != 0) {
				return diff;
			}
		}
		break;
#endif /* INET */
	case AF_INET6:
		if ((diff = pf_addr_compare(&a->gwy.addr, &b->gwy.addr,
		    a->af_gwy)) != 0) {
			return diff;
		}

		if (extfilter < PF_EXTFILTER_EI ||
		    !PF_AZERO(&b->ext_gwy.addr, AF_INET6)) {
			if ((diff = pf_addr_compare(&a->ext_gwy.addr, &b->ext_gwy.addr,
			    a->af_gwy)) != 0) {
				return diff;
			}
		}
		break;
	}

	if (a->app_state && b->app_state) {
		if (a->app_state->compare_ext_gwy &&
		    b->app_state->compare_ext_gwy) {
			diff = (const char *)b->app_state->compare_ext_gwy -
			    (const char *)a->app_state->compare_ext_gwy;
			if (diff != 0) {
				return diff;
			}
			diff = a->app_state->compare_ext_gwy(a->app_state,
			    b->app_state);
			if (diff != 0) {
				return diff;
			}
		}
	}

	return 0;
}

static __inline int
pf_state_compare_id(struct pf_state *a, struct pf_state *b)
{
	if (a->id > b->id) {
		return 1;
	}
	if (a->id < b->id) {
		return -1;
	}
	if (a->creatorid > b->creatorid) {
		return 1;
	}
	if (a->creatorid < b->creatorid) {
		return -1;
	}

	return 0;
}

void
pf_addrcpy(struct pf_addr *dst, struct pf_addr *src, sa_family_t af)
{
	switch (af) {
#if INET
	case AF_INET:
		memcpy(&dst->v4addr, &src->v4addr, sizeof(src->v4addr));
		break;
#endif /* INET */
	case AF_INET6:
		memcpy(&dst->v6addr, &src->v6addr, sizeof(src->v6addr));
		break;
	}
}

struct pf_state *
pf_find_state_byid(struct pf_state_cmp *key)
{
	pf_status.fcounters[FCNT_STATE_SEARCH]++;

	return RB_FIND(pf_state_tree_id, &tree_id,
	           (struct pf_state *)(void *)key);
}

static struct pf_state *
pf_find_state(struct pfi_kif *kif, struct pf_state_key_cmp *key, u_int dir)
{
	struct pf_state_key     *sk = NULL;
	struct pf_state         *s;

	pf_status.fcounters[FCNT_STATE_SEARCH]++;

	switch (dir) {
	case PF_OUT:
		sk = RB_FIND(pf_state_tree_lan_ext, &pf_statetbl_lan_ext,
		    (struct pf_state_key *)key);

		break;
	case PF_IN:

		/*
		 * Generally, a packet can match to
		 * at most 1 state in the GWY table, with the sole exception
		 * of NAT64, where a packet can match with at most 2 states
		 * on the GWY table. This is because, unlike NAT44 or NAT66,
		 * NAT64 forward translation is done on the input, not output.
		 * This means a forwarded packet could cause PF to generate 2 states
		 * on both input and output.
		 *
		 * NAT64 reverse translation is done on input. If a packet
		 * matches NAT64 state on the GWY table, prioritize it
		 * over any IPv4 state on the GWY table.
		 */
		if (pf_state_tree_ext_gwy_nat64_cnt > 0 &&
		    key->af_lan == PF_INET && key->af_gwy == PF_INET) {
			key->af_lan = PF_INET6;
			sk = RB_FIND(pf_state_tree_ext_gwy, &pf_statetbl_ext_gwy,
			    (struct pf_state_key *) key);
			key->af_lan = PF_INET;
		}

		if (sk == NULL) {
			sk = RB_FIND(pf_state_tree_ext_gwy, &pf_statetbl_ext_gwy,
			    (struct pf_state_key *)key);
		}
		/*
		 * NAT64 is done only on input, for packets coming in from
		 * from the LAN side, need to lookup the lan_ext tree.
		 */
		if (sk == NULL) {
			sk = RB_FIND(pf_state_tree_lan_ext,
			    &pf_statetbl_lan_ext,
			    (struct pf_state_key *)key);
			if (sk && sk->af_lan == sk->af_gwy) {
				sk = NULL;
			}
		}
		break;
	default:
		panic("pf_find_state");
	}

	/* list is sorted, if-bound states before floating ones */
	if (sk != NULL) {
		TAILQ_FOREACH(s, &sk->states, next)
		if (s->kif == pfi_all || s->kif == kif) {
			return s;
		}
	}

	return NULL;
}

struct pf_state *
pf_find_state_all(struct pf_state_key_cmp *key, u_int dir, int *more)
{
	struct pf_state_key     *sk = NULL;
	struct pf_state         *s, *ret = NULL;

	pf_status.fcounters[FCNT_STATE_SEARCH]++;

	switch (dir) {
	case PF_OUT:
		sk = RB_FIND(pf_state_tree_lan_ext,
		    &pf_statetbl_lan_ext, (struct pf_state_key *)key);
		break;
	case PF_IN:
		sk = RB_FIND(pf_state_tree_ext_gwy,
		    &pf_statetbl_ext_gwy, (struct pf_state_key *)key);
		/*
		 * NAT64 is done only on input, for packets coming in from
		 * from the LAN side, need to lookup the lan_ext tree.
		 */
		if ((sk == NULL) && pf_nat64_configured) {
			sk = RB_FIND(pf_state_tree_lan_ext,
			    &pf_statetbl_lan_ext,
			    (struct pf_state_key *)key);
			if (sk && sk->af_lan == sk->af_gwy) {
				sk = NULL;
			}
		}
		break;
	default:
		panic("pf_find_state_all");
	}

	if (sk != NULL) {
		ret = TAILQ_FIRST(&sk->states);
		if (more == NULL) {
			return ret;
		}

		TAILQ_FOREACH(s, &sk->states, next)
		(*more)++;
	}

	return ret;
}

static void
pf_init_threshold(struct pf_threshold *threshold,
    u_int32_t limit, u_int32_t seconds)
{
	threshold->limit = limit * PF_THRESHOLD_MULT;
	threshold->seconds = seconds;
	threshold->count = 0;
	threshold->last = pf_time_second();
}

static void
pf_add_threshold(struct pf_threshold *threshold)
{
	u_int32_t t = pf_time_second(), diff = t - threshold->last;

	if (diff >= threshold->seconds) {
		threshold->count = 0;
	} else {
		threshold->count -= threshold->count * diff /
		    threshold->seconds;
	}
	threshold->count += PF_THRESHOLD_MULT;
	threshold->last = t;
}

static int
pf_check_threshold(struct pf_threshold *threshold)
{
	return threshold->count > threshold->limit;
}

static int
pf_src_connlimit(struct pf_state **state)
{
	int bad = 0;
	(*state)->src_node->conn++;
	VERIFY((*state)->src_node->conn != 0);
	(*state)->src.tcp_est = 1;
	pf_add_threshold(&(*state)->src_node->conn_rate);

	if ((*state)->rule.ptr->max_src_conn &&
	    (*state)->rule.ptr->max_src_conn <
	    (*state)->src_node->conn) {
		pf_status.lcounters[LCNT_SRCCONN]++;
		bad++;
	}

	if ((*state)->rule.ptr->max_src_conn_rate.limit &&
	    pf_check_threshold(&(*state)->src_node->conn_rate)) {
		pf_status.lcounters[LCNT_SRCCONNRATE]++;
		bad++;
	}

	if (!bad) {
		return 0;
	}

	if ((*state)->rule.ptr->overload_tbl) {
		struct pfr_addr p;
		u_int32_t       killed = 0;

		pf_status.lcounters[LCNT_OVERLOAD_TABLE]++;
		if (pf_status.debug >= PF_DEBUG_MISC) {
			printf("pf_src_connlimit: blocking address ");
			pf_print_host(&(*state)->src_node->addr, 0,
			    (*state)->state_key->af_lan);
		}

		bzero(&p, sizeof(p));
		p.pfra_af = (*state)->state_key->af_lan;
		switch ((*state)->state_key->af_lan) {
#if INET
		case AF_INET:
			p.pfra_net = 32;
			p.pfra_ip4addr = (*state)->src_node->addr.v4addr;
			break;
#endif /* INET */
		case AF_INET6:
			p.pfra_net = 128;
			p.pfra_ip6addr = (*state)->src_node->addr.v6addr;
			break;
		}

		pfr_insert_kentry((*state)->rule.ptr->overload_tbl,
		    &p, pf_calendar_time_second());

		/* kill existing states if that's required. */
		if ((*state)->rule.ptr->flush) {
			struct pf_state_key *sk;
			struct pf_state *st;

			pf_status.lcounters[LCNT_OVERLOAD_FLUSH]++;
			RB_FOREACH(st, pf_state_tree_id, &tree_id) {
				sk = st->state_key;
				/*
				 * Kill states from this source.  (Only those
				 * from the same rule if PF_FLUSH_GLOBAL is not
				 * set)
				 */
				if (sk->af_lan ==
				    (*state)->state_key->af_lan &&
				    (((*state)->state_key->direction ==
				    PF_OUT &&
				    PF_AEQ(&(*state)->src_node->addr,
				    &sk->lan.addr, sk->af_lan)) ||
				    ((*state)->state_key->direction == PF_IN &&
				    PF_AEQ(&(*state)->src_node->addr,
				    &sk->ext_lan.addr, sk->af_lan))) &&
				    ((*state)->rule.ptr->flush &
				    PF_FLUSH_GLOBAL ||
				    (*state)->rule.ptr == st->rule.ptr)) {
					st->timeout = PFTM_PURGE;
					st->src.state = st->dst.state =
					    TCPS_CLOSED;
					killed++;
				}
			}
			if (pf_status.debug >= PF_DEBUG_MISC) {
				printf(", %u states killed", killed);
			}
		}
		if (pf_status.debug >= PF_DEBUG_MISC) {
			printf("\n");
		}
	}

	/* kill this state */
	(*state)->timeout = PFTM_PURGE;
	(*state)->src.state = (*state)->dst.state = TCPS_CLOSED;
	return 1;
}

int
pf_insert_src_node(struct pf_src_node **sn, struct pf_rule *rule,
    struct pf_addr *src, sa_family_t af)
{
	struct pf_src_node      k;

	if (*sn == NULL) {
		k.af = af;
		PF_ACPY(&k.addr, src, af);
		if (rule->rule_flag & PFRULE_RULESRCTRACK ||
		    rule->rpool.opts & PF_POOL_STICKYADDR) {
			k.rule.ptr = rule;
		} else {
			k.rule.ptr = NULL;
		}
		pf_status.scounters[SCNT_SRC_NODE_SEARCH]++;
		*sn = RB_FIND(pf_src_tree, &tree_src_tracking, &k);
	}
	if (*sn == NULL) {
		if (!rule->max_src_nodes ||
		    rule->src_nodes < rule->max_src_nodes) {
			(*sn) = pool_get(&pf_src_tree_pl, PR_WAITOK);
		} else {
			pf_status.lcounters[LCNT_SRCNODES]++;
		}
		if ((*sn) == NULL) {
			return -1;
		}
		bzero(*sn, sizeof(struct pf_src_node));

		pf_init_threshold(&(*sn)->conn_rate,
		    rule->max_src_conn_rate.limit,
		    rule->max_src_conn_rate.seconds);

		(*sn)->af = af;
		if (rule->rule_flag & PFRULE_RULESRCTRACK ||
		    rule->rpool.opts & PF_POOL_STICKYADDR) {
			(*sn)->rule.ptr = rule;
		} else {
			(*sn)->rule.ptr = NULL;
		}
		PF_ACPY(&(*sn)->addr, src, af);
		if (RB_INSERT(pf_src_tree,
		    &tree_src_tracking, *sn) != NULL) {
			if (pf_status.debug >= PF_DEBUG_MISC) {
				printf("pf: src_tree insert failed: ");
				pf_print_host(&(*sn)->addr, 0, af);
				printf("\n");
			}
			pool_put(&pf_src_tree_pl, *sn);
			*sn = NULL; /* signal the caller that no additional cleanup is needed */
			return -1;
		}
		(*sn)->creation = pf_time_second();
		(*sn)->ruletype = rule->action;
		if ((*sn)->rule.ptr != NULL) {
			(*sn)->rule.ptr->src_nodes++;
		}
		pf_status.scounters[SCNT_SRC_NODE_INSERT]++;
		pf_status.src_nodes++;
	} else {
		if (rule->max_src_states &&
		    (*sn)->states >= rule->max_src_states) {
			pf_status.lcounters[LCNT_SRCSTATES]++;
			return -1;
		}
	}
	return 0;
}

static void
pf_stateins_err(const char *tree, struct pf_state *s, struct pfi_kif *kif)
{
	struct pf_state_key     *sk = s->state_key;

	if (pf_status.debug >= PF_DEBUG_MISC) {
		printf("pf: state insert failed: %s %s ", tree, kif->pfik_name);
		switch (sk->proto) {
		case IPPROTO_TCP:
			printf("TCP");
			break;
		case IPPROTO_UDP:
			printf("UDP");
			break;
		case IPPROTO_ICMP:
			printf("ICMP4");
			break;
		case IPPROTO_ICMPV6:
			printf("ICMP6");
			break;
		default:
			printf("PROTO=%u", sk->proto);
			break;
		}
		printf(" lan: ");
		pf_print_sk_host(&sk->lan, sk->af_lan, sk->proto,
		    sk->proto_variant);
		printf(" gwy: ");
		pf_print_sk_host(&sk->gwy, sk->af_gwy, sk->proto,
		    sk->proto_variant);
		printf(" ext_lan: ");
		pf_print_sk_host(&sk->ext_lan, sk->af_lan, sk->proto,
		    sk->proto_variant);
		printf(" ext_gwy: ");
		pf_print_sk_host(&sk->ext_gwy, sk->af_gwy, sk->proto,
		    sk->proto_variant);
		if (s->sync_flags & PFSTATE_FROMSYNC) {
			printf(" (from sync)");
		}
		printf("\n");
	}
}

static __inline struct pf_state_key *
pf_insert_state_key_ext_gwy(struct pf_state_key *psk)
{
	struct pf_state_key * ret = RB_INSERT(pf_state_tree_ext_gwy,
	    &pf_statetbl_ext_gwy, psk);
	if (!ret && psk->af_lan == PF_INET6 &&
	    psk->af_gwy == PF_INET) {
		pf_state_tree_ext_gwy_nat64_cnt++;
	}
	return ret;
}

static __inline struct pf_state_key *
pf_remove_state_key_ext_gwy(struct pf_state_key *psk)
{
	struct pf_state_key * ret = RB_REMOVE(pf_state_tree_ext_gwy,
	    &pf_statetbl_ext_gwy, psk);
	if (ret && psk->af_lan == PF_INET6 &&
	    psk->af_gwy == PF_INET) {
		pf_state_tree_ext_gwy_nat64_cnt--;
	}
	return ret;
}

int
pf_insert_state(struct pfi_kif *kif, struct pf_state *s)
{
	struct pf_state_key     *cur;
	struct pf_state         *sp;

	VERIFY(s->state_key != NULL);
	s->kif = kif;

	if ((cur = RB_INSERT(pf_state_tree_lan_ext, &pf_statetbl_lan_ext,
	    s->state_key)) != NULL) {
		/* key exists. check for same kif, if none, add to key */
		TAILQ_FOREACH(sp, &cur->states, next)
		if (sp->kif == kif) {           /* collision! */
			pf_stateins_err("tree_lan_ext", s, kif);
			pf_detach_state(s,
			    PF_DT_SKIP_LANEXT | PF_DT_SKIP_EXTGWY);
			return -1;
		}
		pf_detach_state(s, PF_DT_SKIP_LANEXT | PF_DT_SKIP_EXTGWY);
		pf_attach_state(cur, s, kif == pfi_all ? 1 : 0);
	}

	/* if cur != NULL, we already found a state key and attached to it */
	if (cur == NULL &&
	    (cur = pf_insert_state_key_ext_gwy(s->state_key)) != NULL) {
		/* must not happen. we must have found the sk above! */
		pf_stateins_err("tree_ext_gwy", s, kif);
		pf_detach_state(s, PF_DT_SKIP_EXTGWY);
		return -1;
	}

	if (s->id == 0 && s->creatorid == 0) {
		s->id = htobe64(pf_status.stateid++);
		s->creatorid = pf_status.hostid;
	}
	if (RB_INSERT(pf_state_tree_id, &tree_id, s) != NULL) {
		if (pf_status.debug >= PF_DEBUG_MISC) {
			printf("pf: state insert failed: "
			    "id: %016llx creatorid: %08x",
			    be64toh(s->id), ntohl(s->creatorid));
			if (s->sync_flags & PFSTATE_FROMSYNC) {
				printf(" (from sync)");
			}
			printf("\n");
		}
		pf_detach_state(s, 0);
		return -1;
	}
	TAILQ_INSERT_TAIL(&state_list, s, entry_list);
	pf_status.fcounters[FCNT_STATE_INSERT]++;
	pf_status.states++;
	VERIFY(pf_status.states != 0);
	pfi_kif_ref(kif, PFI_KIF_REF_STATE);
#if NPFSYNC
	pfsync_insert_state(s);
#endif
	return 0;
}

static int
pf_purge_thread_cont(int err)
{
#pragma unused(err)
	static u_int32_t nloops = 0;
	int t = 1;      /* 1 second */

	/*
	 * Update coarse-grained networking timestamp (in sec.); the idea
	 * is to piggy-back on the periodic timeout callout to update
	 * the counter returnable via net_uptime().
	 */
	net_update_uptime();

	lck_rw_lock_shared(&pf_perim_lock);
	lck_mtx_lock(&pf_lock);

	/* purge everything if not running */
	if (!pf_status.running) {
		pf_purge_expired_states(pf_status.states);
		pf_purge_expired_fragments();
		pf_purge_expired_src_nodes();

		/* terminate thread (we don't currently do this) */
		if (pf_purge_thread == NULL) {
			lck_mtx_unlock(&pf_lock);
			lck_rw_done(&pf_perim_lock);

			thread_deallocate(current_thread());
			thread_terminate(current_thread());
			/* NOTREACHED */
			return 0;
		} else {
			/* if there's nothing left, sleep w/o timeout */
			if (pf_status.states == 0 &&
			    pf_normalize_isempty() &&
			    RB_EMPTY(&tree_src_tracking)) {
				nloops = 0;
				t = 0;
			}
			goto done;
		}
	}

	/* process a fraction of the state table every second */
	pf_purge_expired_states(1 + (pf_status.states
	    / pf_default_rule.timeout[PFTM_INTERVAL]));

	/* purge other expired types every PFTM_INTERVAL seconds */
	if (++nloops >= pf_default_rule.timeout[PFTM_INTERVAL]) {
		pf_purge_expired_fragments();
		pf_purge_expired_src_nodes();
		nloops = 0;
	}
done:
	lck_mtx_unlock(&pf_lock);
	lck_rw_done(&pf_perim_lock);

	(void) tsleep0(pf_purge_thread_fn, PWAIT, "pf_purge_cont",
	    t * hz, pf_purge_thread_cont);
	/* NOTREACHED */
	VERIFY(0);

	return 0;
}

void
pf_purge_thread_fn(void *v, wait_result_t w)
{
#pragma unused(v, w)
	(void) tsleep0(pf_purge_thread_fn, PWAIT, "pf_purge", 0,
	    pf_purge_thread_cont);
	/*
	 * tsleep0() shouldn't have returned as PCATCH was not set;
	 * therefore assert in this case.
	 */
	VERIFY(0);
}

u_int64_t
pf_state_expires(const struct pf_state *state)
{
	u_int32_t       t;
	u_int32_t       start;
	u_int32_t       end;
	u_int32_t       states;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	/* handle all PFTM_* > PFTM_MAX here */
	if (state->timeout == PFTM_PURGE) {
		return pf_time_second();
	}

	VERIFY(state->timeout != PFTM_UNLINKED);
	VERIFY(state->timeout < PFTM_MAX);
	t = state->rule.ptr->timeout[state->timeout];
	if (!t) {
		t = pf_default_rule.timeout[state->timeout];
	}
	start = state->rule.ptr->timeout[PFTM_ADAPTIVE_START];
	if (start) {
		end = state->rule.ptr->timeout[PFTM_ADAPTIVE_END];
		states = state->rule.ptr->states;
	} else {
		start = pf_default_rule.timeout[PFTM_ADAPTIVE_START];
		end = pf_default_rule.timeout[PFTM_ADAPTIVE_END];
		states = pf_status.states;
	}
	if (end && states > start && start < end) {
		if (states < end) {
			return state->expire + t * (end - states) /
			       (end - start);
		} else {
			return pf_time_second();
		}
	}
	return state->expire + t;
}

void
pf_purge_expired_src_nodes(void)
{
	struct pf_src_node              *cur, *next;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	for (cur = RB_MIN(pf_src_tree, &tree_src_tracking); cur; cur = next) {
		next = RB_NEXT(pf_src_tree, &tree_src_tracking, cur);

		if (cur->states <= 0 && cur->expire <= pf_time_second()) {
			if (cur->rule.ptr != NULL) {
				cur->rule.ptr->src_nodes--;
				if (cur->rule.ptr->states <= 0 &&
				    cur->rule.ptr->max_src_nodes <= 0) {
					pf_rm_rule(NULL, cur->rule.ptr);
				}
			}
			RB_REMOVE(pf_src_tree, &tree_src_tracking, cur);
			pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
			pf_status.src_nodes--;
			pool_put(&pf_src_tree_pl, cur);
		}
	}
}

void
pf_src_tree_remove_state(struct pf_state *s)
{
	u_int32_t t;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	if (s->src_node != NULL) {
		if (s->src.tcp_est) {
			VERIFY(s->src_node->conn > 0);
			--s->src_node->conn;
		}
		VERIFY(s->src_node->states > 0);
		if (--s->src_node->states <= 0) {
			t = s->rule.ptr->timeout[PFTM_SRC_NODE];
			if (!t) {
				t = pf_default_rule.timeout[PFTM_SRC_NODE];
			}
			s->src_node->expire = pf_time_second() + t;
		}
	}
	if (s->nat_src_node != s->src_node && s->nat_src_node != NULL) {
		VERIFY(s->nat_src_node->states > 0);
		if (--s->nat_src_node->states <= 0) {
			t = s->rule.ptr->timeout[PFTM_SRC_NODE];
			if (!t) {
				t = pf_default_rule.timeout[PFTM_SRC_NODE];
			}
			s->nat_src_node->expire = pf_time_second() + t;
		}
	}
	s->src_node = s->nat_src_node = NULL;
}

void
pf_unlink_state(struct pf_state *cur)
{
	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	if (cur->src.state == PF_TCPS_PROXY_DST) {
		pf_send_tcp(cur->rule.ptr, cur->state_key->af_lan,
		    &cur->state_key->ext_lan.addr, &cur->state_key->lan.addr,
		    cur->state_key->ext_lan.xport.port,
		    cur->state_key->lan.xport.port,
		    cur->src.seqhi, cur->src.seqlo + 1,
		    TH_RST | TH_ACK, 0, 0, 0, 1, cur->tag, NULL, NULL);
	}

	hook_runloop(&cur->unlink_hooks, HOOK_REMOVE | HOOK_FREE);
	RB_REMOVE(pf_state_tree_id, &tree_id, cur);
#if NPFSYNC
	if (cur->creatorid == pf_status.hostid) {
		pfsync_delete_state(cur);
	}
#endif
	cur->timeout = PFTM_UNLINKED;
	pf_src_tree_remove_state(cur);
	pf_detach_state(cur, 0);
}

/* callers should be at splpf and hold the
 * write_lock on pf_consistency_lock */
void
pf_free_state(struct pf_state *cur)
{
	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);
#if NPFSYNC
	if (pfsyncif != NULL &&
	    (pfsyncif->sc_bulk_send_next == cur ||
	    pfsyncif->sc_bulk_terminator == cur)) {
		return;
	}
#endif
	VERIFY(cur->timeout == PFTM_UNLINKED);
	VERIFY(cur->rule.ptr->states > 0);
	if (--cur->rule.ptr->states <= 0 &&
	    cur->rule.ptr->src_nodes <= 0) {
		pf_rm_rule(NULL, cur->rule.ptr);
	}
	if (cur->nat_rule.ptr != NULL) {
		VERIFY(cur->nat_rule.ptr->states > 0);
		if (--cur->nat_rule.ptr->states <= 0 &&
		    cur->nat_rule.ptr->src_nodes <= 0) {
			pf_rm_rule(NULL, cur->nat_rule.ptr);
		}
	}
	if (cur->anchor.ptr != NULL) {
		VERIFY(cur->anchor.ptr->states > 0);
		if (--cur->anchor.ptr->states <= 0) {
			pf_rm_rule(NULL, cur->anchor.ptr);
		}
	}
	pf_normalize_tcp_cleanup(cur);
	pfi_kif_unref(cur->kif, PFI_KIF_REF_STATE);
	TAILQ_REMOVE(&state_list, cur, entry_list);
	if (cur->tag) {
		pf_tag_unref(cur->tag);
	}
#if SKYWALK
	netns_release(&cur->nstoken);
#endif
	pool_put(&pf_state_pl, cur);
	pf_status.fcounters[FCNT_STATE_REMOVALS]++;
	VERIFY(pf_status.states > 0);
	pf_status.states--;
}

void
pf_purge_expired_states(u_int32_t maxcheck)
{
	static struct pf_state  *cur = NULL;
	struct pf_state         *next;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	while (maxcheck--) {
		/* wrap to start of list when we hit the end */
		if (cur == NULL) {
			cur = TAILQ_FIRST(&state_list);
			if (cur == NULL) {
				break;  /* list empty */
			}
		}

		/* get next state, as cur may get deleted */
		next = TAILQ_NEXT(cur, entry_list);

		if (cur->timeout == PFTM_UNLINKED) {
			pf_free_state(cur);
		} else if (pf_state_expires(cur) <= pf_time_second()) {
			/* unlink and free expired state */
			pf_unlink_state(cur);
			pf_free_state(cur);
		}
		cur = next;
	}
}

int
pf_tbladdr_setup(struct pf_ruleset *rs, struct pf_addr_wrap *aw)
{
	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	if (aw->type != PF_ADDR_TABLE) {
		return 0;
	}
	if ((aw->p.tbl = pfr_attach_table(rs, __unsafe_null_terminated_from_indexable(aw->v.tblname))) == NULL) {
		return 1;
	}
	return 0;
}

void
pf_tbladdr_remove(struct pf_addr_wrap *aw)
{
	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	if (aw->type != PF_ADDR_TABLE || aw->p.tbl == NULL) {
		return;
	}
	pfr_detach_table(aw->p.tbl);
	aw->p.tbl = NULL;
}

void
pf_tbladdr_copyout(struct pf_addr_wrap *aw)
{
	struct pfr_ktable *kt = aw->p.tbl;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	if (aw->type != PF_ADDR_TABLE || kt == NULL) {
		return;
	}
	if (!(kt->pfrkt_flags & PFR_TFLAG_ACTIVE) && kt->pfrkt_root != NULL) {
		kt = kt->pfrkt_root;
	}
	aw->p.tbl = NULL;
	aw->p.tblcnt = (kt->pfrkt_flags & PFR_TFLAG_ACTIVE) ?
	    kt->pfrkt_cnt : -1;
}

static void
pf_print_addr(struct pf_addr *addr, sa_family_t af)
{
	switch (af) {
#if INET
	case AF_INET: {
		u_int32_t a = ntohl(addr->addr32[0]);
		printf("%u.%u.%u.%u", (a >> 24) & 255, (a >> 16) & 255,
		    (a >> 8) & 255, a & 255);
		break;
	}
#endif /* INET */
	case AF_INET6: {
		u_int16_t b;
		u_int8_t i, curstart = 255, curend = 0,
		    maxstart = 0, maxend = 0;
		for (i = 0; i < 8; i++) {
			if (!addr->addr16[i]) {
				if (curstart == 255) {
					curstart = i;
				} else {
					curend = i;
				}
			} else {
				if (curstart) {
					if ((curend - curstart) >
					    (maxend - maxstart)) {
						maxstart = curstart;
						maxend = curend;
						curstart = 255;
					}
				}
			}
		}
		for (i = 0; i < 8; i++) {
			if (i >= maxstart && i <= maxend) {
				if (maxend != 7) {
					if (i == maxstart) {
						printf(":");
					}
				} else {
					if (i == maxend) {
						printf(":");
					}
				}
			} else {
				b = ntohs(addr->addr16[i]);
				printf("%x", b);
				if (i < 7) {
					printf(":");
				}
			}
		}
		break;
	}
	}
}

static void
pf_print_sk_host(struct pf_state_host *sh, sa_family_t af, int proto,
    u_int8_t proto_variant)
{
	pf_print_addr(&sh->addr, af);

	switch (proto) {
	case IPPROTO_ESP:
		if (sh->xport.spi) {
			printf("[%08x]", ntohl(sh->xport.spi));
		}
		break;

	case IPPROTO_GRE:
		if (proto_variant == PF_GRE_PPTP_VARIANT) {
			printf("[%u]", ntohs(sh->xport.call_id));
		}
		break;

	case IPPROTO_TCP:
	case IPPROTO_UDP:
		printf("[%u]", ntohs(sh->xport.port));
		break;

	default:
		break;
	}
}

static void
pf_print_host(struct pf_addr *addr, u_int16_t p, sa_family_t af)
{
	pf_print_addr(addr, af);
	if (p) {
		printf("[%u]", ntohs(p));
	}
}

void
pf_print_state(struct pf_state *s)
{
	struct pf_state_key *sk = s->state_key;
	switch (sk->proto) {
	case IPPROTO_ESP:
		printf("ESP ");
		break;
	case IPPROTO_GRE:
		printf("GRE%u ", sk->proto_variant);
		break;
	case IPPROTO_TCP:
		printf("TCP ");
		break;
	case IPPROTO_UDP:
		printf("UDP ");
		break;
	case IPPROTO_ICMP:
		printf("ICMP ");
		break;
	case IPPROTO_ICMPV6:
		printf("ICMPV6 ");
		break;
	default:
		printf("%u ", sk->proto);
		break;
	}
	pf_print_sk_host(&sk->lan, sk->af_lan, sk->proto, sk->proto_variant);
	printf(" ");
	pf_print_sk_host(&sk->gwy, sk->af_gwy, sk->proto, sk->proto_variant);
	printf(" ");
	pf_print_sk_host(&sk->ext_lan, sk->af_lan, sk->proto,
	    sk->proto_variant);
	printf(" ");
	pf_print_sk_host(&sk->ext_gwy, sk->af_gwy, sk->proto,
	    sk->proto_variant);
	printf(" [lo=%u high=%u win=%u modulator=%u", s->src.seqlo,
	    s->src.seqhi, s->src.max_win, s->src.seqdiff);
	if (s->src.wscale && s->dst.wscale) {
		printf(" wscale=%u", s->src.wscale & PF_WSCALE_MASK);
	}
	printf("]");
	printf(" [lo=%u high=%u win=%u modulator=%u", s->dst.seqlo,
	    s->dst.seqhi, s->dst.max_win, s->dst.seqdiff);
	if (s->src.wscale && s->dst.wscale) {
		printf(" wscale=%u", s->dst.wscale & PF_WSCALE_MASK);
	}
	printf("]");
	printf(" %u:%u", s->src.state, s->dst.state);
}

void
pf_print_flags(u_int8_t f)
{
	if (f) {
		printf(" ");
	}
	if (f & TH_FIN) {
		printf("F");
	}
	if (f & TH_SYN) {
		printf("S");
	}
	if (f & TH_RST) {
		printf("R");
	}
	if (f & TH_PUSH) {
		printf("P");
	}
	if (f & TH_ACK) {
		printf("A");
	}
	if (f & TH_URG) {
		printf("U");
	}
	if (f & TH_ECE) {
		printf("E");
	}
	if (f & TH_CWR) {
		printf("W");
	}
}

#define PF_SET_SKIP_STEPS(i)                                    \
	do {                                                    \
	        while (head[i] != cur) {                        \
	                head[i]->skip[i].ptr = cur;             \
	                head[i] = TAILQ_NEXT(head[i], entries); \
	        }                                               \
	} while (0)

void
pf_calc_skip_steps(struct pf_rulequeue *rules)
{
	struct pf_rule *cur, *prev, *head[PF_SKIP_COUNT];
	int i;

	cur = TAILQ_FIRST(rules);
	prev = cur;
	for (i = 0; i < PF_SKIP_COUNT; ++i) {
		head[i] = cur;
	}
	while (cur != NULL) {
		if (cur->kif != prev->kif || cur->ifnot != prev->ifnot) {
			PF_SET_SKIP_STEPS(PF_SKIP_IFP);
		}
		if (cur->direction != prev->direction) {
			PF_SET_SKIP_STEPS(PF_SKIP_DIR);
		}
		if (cur->af != prev->af) {
			PF_SET_SKIP_STEPS(PF_SKIP_AF);
		}
		if (cur->proto != prev->proto) {
			PF_SET_SKIP_STEPS(PF_SKIP_PROTO);
		}
		if (cur->src.neg != prev->src.neg ||
		    pf_addr_wrap_neq(&cur->src.addr, &prev->src.addr)) {
			PF_SET_SKIP_STEPS(PF_SKIP_SRC_ADDR);
		}
		{
			union pf_rule_xport *cx = &cur->src.xport;
			union pf_rule_xport *px = &prev->src.xport;

			switch (cur->proto) {
			case IPPROTO_GRE:
			case IPPROTO_ESP:
				PF_SET_SKIP_STEPS(PF_SKIP_SRC_PORT);
				break;
			default:
				if (prev->proto == IPPROTO_GRE ||
				    prev->proto == IPPROTO_ESP ||
				    cx->range.op != px->range.op ||
				    cx->range.port[0] != px->range.port[0] ||
				    cx->range.port[1] != px->range.port[1]) {
					PF_SET_SKIP_STEPS(PF_SKIP_SRC_PORT);
				}
				break;
			}
		}
		if (cur->dst.neg != prev->dst.neg ||
		    pf_addr_wrap_neq(&cur->dst.addr, &prev->dst.addr)) {
			PF_SET_SKIP_STEPS(PF_SKIP_DST_ADDR);
		}
		{
			union pf_rule_xport *cx = &cur->dst.xport;
			union pf_rule_xport *px = &prev->dst.xport;

			switch (cur->proto) {
			case IPPROTO_GRE:
				if (cur->proto != prev->proto ||
				    cx->call_id != px->call_id) {
					PF_SET_SKIP_STEPS(PF_SKIP_DST_PORT);
				}
				break;
			case IPPROTO_ESP:
				if (cur->proto != prev->proto ||
				    cx->spi != px->spi) {
					PF_SET_SKIP_STEPS(PF_SKIP_DST_PORT);
				}
				break;
			default:
				if (prev->proto == IPPROTO_GRE ||
				    prev->proto == IPPROTO_ESP ||
				    cx->range.op != px->range.op ||
				    cx->range.port[0] != px->range.port[0] ||
				    cx->range.port[1] != px->range.port[1]) {
					PF_SET_SKIP_STEPS(PF_SKIP_DST_PORT);
				}
				break;
			}
		}

		prev = cur;
		cur = TAILQ_NEXT(cur, entries);
	}
	for (i = 0; i < PF_SKIP_COUNT; ++i) {
		PF_SET_SKIP_STEPS(i);
	}
}

u_int32_t
pf_calc_state_key_flowhash(struct pf_state_key *sk)
{
#if SKYWALK
	uint32_t flowid;
	struct flowidns_flow_key fk;

	VERIFY(sk->flowsrc == FLOWSRC_PF);
	bzero(&fk, sizeof(fk));
	_CASSERT(sizeof(sk->lan.addr) == sizeof(fk.ffk_laddr));
	_CASSERT(sizeof(sk->ext_lan.addr) == sizeof(fk.ffk_laddr));
	bcopy(&sk->lan.addr, &fk.ffk_laddr, sizeof(fk.ffk_laddr));
	bcopy(&sk->ext_lan.addr, &fk.ffk_raddr, sizeof(fk.ffk_raddr));
	fk.ffk_af = sk->af_lan;
	fk.ffk_proto = sk->proto;

	switch (sk->proto) {
	case IPPROTO_ESP:
	case IPPROTO_AH:
		fk.ffk_spi = sk->lan.xport.spi;
		break;
	default:
		if (sk->lan.xport.spi <= sk->ext_lan.xport.spi) {
			fk.ffk_lport = sk->lan.xport.port;
			fk.ffk_rport = sk->ext_lan.xport.port;
		} else {
			fk.ffk_lport = sk->ext_lan.xport.port;
			fk.ffk_rport = sk->lan.xport.port;
		}
		break;
	}

	flowidns_allocate_flowid(FLOWIDNS_DOMAIN_PF, &fk, &flowid);
	return flowid;

#else /* !SKYWALK */

	struct pf_flowhash_key fh __attribute__((aligned(8)));
	uint32_t flowhash = 0;

	bzero(&fh, sizeof(fh));
	if (PF_ALEQ(&sk->lan.addr, &sk->ext_lan.addr, sk->af_lan)) {
		bcopy(&sk->lan.addr, &fh.ap1.addr, sizeof(fh.ap1.addr));
		bcopy(&sk->ext_lan.addr, &fh.ap2.addr, sizeof(fh.ap2.addr));
	} else {
		bcopy(&sk->ext_lan.addr, &fh.ap1.addr, sizeof(fh.ap1.addr));
		bcopy(&sk->lan.addr, &fh.ap2.addr, sizeof(fh.ap2.addr));
	}
	if (sk->lan.xport.spi <= sk->ext_lan.xport.spi) {
		fh.ap1.xport.spi = sk->lan.xport.spi;
		fh.ap2.xport.spi = sk->ext_lan.xport.spi;
	} else {
		fh.ap1.xport.spi = sk->ext_lan.xport.spi;
		fh.ap2.xport.spi = sk->lan.xport.spi;
	}
	fh.af = sk->af_lan;
	fh.proto = sk->proto;

try_again:
	flowhash = net_flowhash(&fh, sizeof(fh), pf_hash_seed);
	if (flowhash == 0) {
		/* try to get a non-zero flowhash */
		pf_hash_seed = RandomULong();
		goto try_again;
	}

	return flowhash;

#endif /* !SKYWALK */
}

static int
pf_addr_wrap_neq(struct pf_addr_wrap *aw1, struct pf_addr_wrap *aw2)
{
	if (aw1->type != aw2->type) {
		return 1;
	}
	switch (aw1->type) {
	case PF_ADDR_ADDRMASK:
	case PF_ADDR_RANGE:
		if (PF_ANEQ(&aw1->v.a.addr, &aw2->v.a.addr, AF_INET6)) {
			return 1;
		}
		if (PF_ANEQ(&aw1->v.a.mask, &aw2->v.a.mask, AF_INET6)) {
			return 1;
		}
		return 0;
	case PF_ADDR_DYNIFTL:
		return aw1->p.dyn == NULL || aw2->p.dyn == NULL ||
		       aw1->p.dyn->pfid_kt != aw2->p.dyn->pfid_kt;
	case PF_ADDR_NOROUTE:
	case PF_ADDR_URPFFAILED:
		return 0;
	case PF_ADDR_TABLE:
		return aw1->p.tbl != aw2->p.tbl;
	case PF_ADDR_RTLABEL:
		return aw1->v.rtlabel != aw2->v.rtlabel;
	default:
		printf("invalid address type: %d\n", aw1->type);
		return 1;
	}
}

u_int16_t
pf_cksum_fixup(u_int16_t cksum, u_int16_t old, u_int16_t new, u_int8_t udp)
{
	return nat464_cksum_fixup(cksum, old, new, udp);
}

/*
 * change ip address & port
 * dir	: packet direction
 * a	: address to be changed
 * p	: port to be changed
 * ic	: ip header checksum
 * pc	: protocol checksum
 * an	: new ip address
 * pn	: new port
 * u	: should be 1 if UDP packet else 0
 * af	: address family of the packet
 * afn	: address family of the new address
 * ua	: should be 1 if ip address needs to be updated in the packet else
 *	  only the checksum is recalculated & updated.
 */
static __attribute__((noinline)) void
pf_change_ap(int dir, pbuf_t *pbuf, struct pf_addr *a, u_int16_t *p,
    u_int16_t *ic, u_int16_t *pc, struct pf_addr *an, u_int16_t pn,
    u_int8_t u, sa_family_t af, sa_family_t afn, int ua)
{
	struct pf_addr  ao;
	u_int16_t       po = *p;

	PF_ACPY(&ao, a, af);
	if (ua) {
		PF_ACPY(a, an, afn);
	}

	*p = pn;

	switch (af) {
#if INET
	case AF_INET:
		switch (afn) {
		case AF_INET:
			*ic = pf_cksum_fixup(pf_cksum_fixup(*ic,
			    ao.addr16[0], an->addr16[0], 0),
			    ao.addr16[1], an->addr16[1], 0);
			*p = pn;
			/*
			 * If the packet is originated from an ALG on the NAT gateway
			 * (source address is loopback or local), in which case the
			 * TCP/UDP checksum field contains the pseudo header checksum
			 * that's not yet complemented.
			 * In that case we do not need to fixup the checksum for port
			 * translation as the pseudo header checksum doesn't include ports.
			 *
			 * A packet generated locally will have UDP/TCP CSUM flag
			 * set (gets set in protocol output).
			 *
			 * It should be noted that the fixup doesn't do anything if the
			 * checksum is 0.
			 */
			if (dir == PF_OUT && pbuf != NULL &&
			    (*pbuf->pb_csum_flags & (CSUM_TCP | CSUM_UDP))) {
				/* Pseudo-header checksum does not include ports */
				*pc = ~pf_cksum_fixup(pf_cksum_fixup(~*pc,
				    ao.addr16[0], an->addr16[0], u),
				    ao.addr16[1], an->addr16[1], u);
			} else {
				*pc =
				    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
					    *pc, ao.addr16[0], an->addr16[0], u),
				    ao.addr16[1], an->addr16[1], u),
				    po, pn, u);
			}
			break;
		case AF_INET6:
			*p = pn;
			*pc = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
				    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(

					    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(*pc,
					    ao.addr16[0], an->addr16[0], u),
					    ao.addr16[1], an->addr16[1], u),
					    0, an->addr16[2], u),
					    0, an->addr16[3], u),
				    0, an->addr16[4], u),
				    0, an->addr16[5], u),
				    0, an->addr16[6], u),
			    0, an->addr16[7], u),
			    po, pn, u);
			break;
		}
		break;
#endif /* INET */
	case AF_INET6:
		switch (afn) {
		case AF_INET6:
			/*
			 * If the packet is originated from an ALG on the NAT gateway
			 * (source address is loopback or local), in which case the
			 * TCP/UDP checksum field contains the pseudo header checksum
			 * that's not yet complemented.
			 * A packet generated locally
			 * will have UDP/TCP CSUM flag set (gets set in protocol
			 * output).
			 */
			if (dir == PF_OUT && pbuf != NULL &&
			    (*pbuf->pb_csum_flags & (CSUM_TCPIPV6 |
			    CSUM_UDPIPV6))) {
				/* Pseudo-header checksum does not include ports */
				*pc =
				    ~pf_cksum_fixup(pf_cksum_fixup(
					    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
						    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
							    ~*pc,
							    ao.addr16[0], an->addr16[0], u),
						    ao.addr16[1], an->addr16[1], u),
						    ao.addr16[2], an->addr16[2], u),
						    ao.addr16[3], an->addr16[3], u),
					    ao.addr16[4], an->addr16[4], u),
					    ao.addr16[5], an->addr16[5], u),
					    ao.addr16[6], an->addr16[6], u),
				    ao.addr16[7], an->addr16[7], u);
			} else {
				*pc =
				    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
					    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
						    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
							    *pc,
							    ao.addr16[0], an->addr16[0], u),
						    ao.addr16[1], an->addr16[1], u),
						    ao.addr16[2], an->addr16[2], u),
						    ao.addr16[3], an->addr16[3], u),
					    ao.addr16[4], an->addr16[4], u),
					    ao.addr16[5], an->addr16[5], u),
					    ao.addr16[6], an->addr16[6], u),
				    ao.addr16[7], an->addr16[7], u),
				    po, pn, u);
			}
			break;
#ifdef INET
		case AF_INET:
			*pc = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
				    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
					    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(*pc,
					    ao.addr16[0], an->addr16[0], u),
					    ao.addr16[1], an->addr16[1], u),
					    ao.addr16[2], 0, u),
					    ao.addr16[3], 0, u),
				    ao.addr16[4], 0, u),
				    ao.addr16[5], 0, u),
				    ao.addr16[6], 0, u),
			    ao.addr16[7], 0, u),
			    po, pn, u);
			break;
#endif /* INET */
		}
		break;
	}
}


/* Changes a u_int32_t.  Uses a void * so there are no align restrictions */
void
pf_change_a(void *a, u_int16_t *c, u_int32_t an, u_int8_t u)
{
	u_int32_t       ao;

	memcpy(&ao, (uint32_t *)a, sizeof(ao));
	memcpy((uint32_t *)a, &an, sizeof(u_int32_t));
	*c = pf_cksum_fixup(pf_cksum_fixup(*c, ao / 65536, an / 65536, u),
	    ao % 65536, an % 65536, u);
}

static __attribute__((noinline)) void
pf_change_a6(struct pf_addr *a, u_int16_t *c, struct pf_addr *an, u_int8_t u)
{
	struct pf_addr  ao;

	PF_ACPY(&ao, a, AF_INET6);
	PF_ACPY(a, an, AF_INET6);

	*c = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
		    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
			    pf_cksum_fixup(pf_cksum_fixup(*c,
			    ao.addr16[0], an->addr16[0], u),
			    ao.addr16[1], an->addr16[1], u),
			    ao.addr16[2], an->addr16[2], u),
		    ao.addr16[3], an->addr16[3], u),
		    ao.addr16[4], an->addr16[4], u),
		    ao.addr16[5], an->addr16[5], u),
	    ao.addr16[6], an->addr16[6], u),
	    ao.addr16[7], an->addr16[7], u);
}

static __attribute__((noinline)) void
pf_change_addr(struct pf_addr *a, u_int16_t *c, struct pf_addr *an, u_int8_t u,
    sa_family_t af, sa_family_t afn)
{
	struct pf_addr  ao;

	if (af != afn) {
		PF_ACPY(&ao, a, af);
		PF_ACPY(a, an, afn);
	}

	switch (af) {
	case AF_INET:
		switch (afn) {
		case AF_INET:
			pf_change_a(a, c, an->v4addr.s_addr, u);
			break;
		case AF_INET6:
			*c = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
				    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
					    pf_cksum_fixup(pf_cksum_fixup(*c,
					    ao.addr16[0], an->addr16[0], u),
					    ao.addr16[1], an->addr16[1], u),
					    0, an->addr16[2], u),
				    0, an->addr16[3], u),
				    0, an->addr16[4], u),
				    0, an->addr16[5], u),
			    0, an->addr16[6], u),
			    0, an->addr16[7], u);
			break;
		}
		break;
	case AF_INET6:
		switch (afn) {
		case AF_INET:
			*c = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
				    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
					    pf_cksum_fixup(pf_cksum_fixup(*c,
					    ao.addr16[0], an->addr16[0], u),
					    ao.addr16[1], an->addr16[1], u),
					    ao.addr16[2], 0, u),
				    ao.addr16[3], 0, u),
				    ao.addr16[4], 0, u),
				    ao.addr16[5], 0, u),
			    ao.addr16[6], 0, u),
			    ao.addr16[7], 0, u);
			break;
		case AF_INET6:
			pf_change_a6(a, c, an, u);
			break;
		}
		break;
	}
}

static __attribute__((noinline)) void
pf_change_icmp(struct pf_addr *ia, u_int16_t *ip, struct pf_addr *oa,
    struct pf_addr *na, u_int16_t np, u_int16_t *pc, u_int16_t *h2c,
    u_int16_t *ic, u_int16_t *hc, u_int8_t u, sa_family_t af)
{
	struct pf_addr  oia, ooa;

	PF_ACPY(&oia, ia, af);
	PF_ACPY(&ooa, oa, af);

	/* Change inner protocol port, fix inner protocol checksum. */
	if (ip != NULL) {
		u_int16_t       oip = *ip;
		u_int32_t       opc = 0;

		if (pc != NULL) {
			opc = *pc;
		}
		*ip = np;
		if (pc != NULL) {
			*pc = pf_cksum_fixup(*pc, oip, *ip, u);
		}
		*ic = pf_cksum_fixup(*ic, oip, *ip, 0);
		if (pc != NULL) {
			*ic = pf_cksum_fixup(*ic, opc, *pc, 0);
		}
	}
	/* Change inner ip address, fix inner ip and icmp checksums. */
	PF_ACPY(ia, na, af);
	switch (af) {
#if INET
	case AF_INET: {
		u_int32_t        oh2c = *h2c;

		*h2c = pf_cksum_fixup(pf_cksum_fixup(*h2c,
		    oia.addr16[0], ia->addr16[0], 0),
		    oia.addr16[1], ia->addr16[1], 0);
		*ic = pf_cksum_fixup(pf_cksum_fixup(*ic,
		    oia.addr16[0], ia->addr16[0], 0),
		    oia.addr16[1], ia->addr16[1], 0);
		*ic = pf_cksum_fixup(*ic, oh2c, *h2c, 0);
		break;
	}
#endif /* INET */
	case AF_INET6:
		*ic = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
			    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
				    pf_cksum_fixup(pf_cksum_fixup(*ic,
				    oia.addr16[0], ia->addr16[0], u),
				    oia.addr16[1], ia->addr16[1], u),
				    oia.addr16[2], ia->addr16[2], u),
			    oia.addr16[3], ia->addr16[3], u),
			    oia.addr16[4], ia->addr16[4], u),
			    oia.addr16[5], ia->addr16[5], u),
		    oia.addr16[6], ia->addr16[6], u),
		    oia.addr16[7], ia->addr16[7], u);
		break;
	}
	/* Change outer ip address, fix outer ip or icmpv6 checksum. */
	PF_ACPY(oa, na, af);
	switch (af) {
#if INET
	case AF_INET:
		*hc = pf_cksum_fixup(pf_cksum_fixup(*hc,
		    ooa.addr16[0], oa->addr16[0], 0),
		    ooa.addr16[1], oa->addr16[1], 0);
		break;
#endif /* INET */
	case AF_INET6:
		*ic = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
			    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
				    pf_cksum_fixup(pf_cksum_fixup(*ic,
				    ooa.addr16[0], oa->addr16[0], u),
				    ooa.addr16[1], oa->addr16[1], u),
				    ooa.addr16[2], oa->addr16[2], u),
			    ooa.addr16[3], oa->addr16[3], u),
			    ooa.addr16[4], oa->addr16[4], u),
			    ooa.addr16[5], oa->addr16[5], u),
		    ooa.addr16[6], oa->addr16[6], u),
		    ooa.addr16[7], oa->addr16[7], u);
		break;
	}
}


/*
 * Need to modulate the sequence numbers in the TCP SACK option
 * (credits to Krzysztof Pfaff for report and patch)
 */
static __attribute__((noinline)) int
pf_modulate_sack(pbuf_t *pbuf, int off, struct pf_pdesc *pd,
    struct tcphdr *th, struct pf_state_peer *dst)
{
	int hlen = (th->th_off << 2) - sizeof(*th), thoptlen = hlen;
	u_int8_t opts[MAX_TCPOPTLEN], *opt = opts;
	int copyback = 0, i, olen;
	struct sackblk sack;

#define TCPOLEN_SACKLEN (TCPOLEN_SACK + 2)
	if (hlen < TCPOLEN_SACKLEN ||
	    !pf_pull_hdr(pbuf, off + sizeof(*th), opts, sizeof(opts), hlen, NULL, NULL, pd->af)) {
		return 0;
	}

	while (hlen >= TCPOLEN_SACKLEN) {
		olen = opt[1];
		switch (*opt) {
		case TCPOPT_EOL:        /* FALLTHROUGH */
		case TCPOPT_NOP:
			opt++;
			hlen--;
			break;
		case TCPOPT_SACK:
			if (olen > hlen) {
				olen = hlen;
			}
			if (olen >= TCPOLEN_SACKLEN) {
				for (i = 2; i + TCPOLEN_SACK <= olen;
				    i += TCPOLEN_SACK) {
					memcpy(&sack, &opt[i], sizeof(sack));
					pf_change_a(&sack.start, &th->th_sum,
					    htonl(ntohl(sack.start) -
					    dst->seqdiff), 0);
					pf_change_a(&sack.end, &th->th_sum,
					    htonl(ntohl(sack.end) -
					    dst->seqdiff), 0);
					memcpy(&opt[i], &sack, sizeof(sack));
				}
				copyback = off + sizeof(*th) + thoptlen;
			}
			OS_FALLTHROUGH;
		default:
			if (olen < 2) {
				olen = 2;
			}
			hlen -= olen;
			opt += olen;
		}
	}

	if (copyback) {
		if (pf_lazy_makewritable(pd, pbuf, copyback) == NULL) {
			return -1;
		}
		pbuf_copy_back(pbuf, off + sizeof(*th), thoptlen, opts, sizeof(opts));
	}
	return copyback;
}

/*
 * XXX
 *
 * The following functions (pf_send_tcp and pf_send_icmp) are somewhat
 * special in that they originate "spurious" packets rather than
 * filter/NAT existing packets. As such, they're not a great fit for
 * the 'pbuf' shim, which assumes the underlying packet buffers are
 * allocated elsewhere.
 *
 * Since these functions are rarely used, we'll carry on allocating mbufs
 * and passing them to the IP stack for eventual routing.
 */
static __attribute__((noinline)) void
pf_send_tcp(const struct pf_rule *r, sa_family_t af,
    const struct pf_addr *saddr, const struct pf_addr *daddr,
    u_int16_t sport, u_int16_t dport, u_int32_t seq, u_int32_t ack,
    u_int8_t flags, u_int16_t win, u_int16_t mss, u_int8_t ttl, int tag,
    u_int16_t rtag, struct ether_header *eh, struct ifnet *ifp)
{
#pragma unused(eh, ifp)
	struct mbuf     *m;
	int              len, tlen;
#if INET
	struct ip       *h = NULL;
#endif /* INET */
	struct ip6_hdr  *h6 = NULL;
	struct tcphdr   *th = NULL;
	char            *opt;
	struct pf_mtag  *pf_mtag;

	/* maximum segment size tcp option */
	tlen = sizeof(struct tcphdr);
	if (mss) {
		tlen += 4;
	}

	switch (af) {
#if INET
	case AF_INET:
		len = sizeof(struct ip) + tlen;
		break;
#endif /* INET */
	case AF_INET6:
		len = sizeof(struct ip6_hdr) + tlen;
		break;
	default:
		panic("pf_send_tcp: not AF_INET or AF_INET6!");
		return;
	}

	/* create outgoing mbuf */
	m = m_gethdr(M_DONTWAIT, MT_HEADER);
	if (m == NULL) {
		return;
	}

	if ((pf_mtag = pf_get_mtag(m)) == NULL) {
		return;
	}

	if (tag) {
		pf_mtag->pftag_flags |= PF_TAG_GENERATED;
	}
	pf_mtag->pftag_tag = rtag;

	if (r != NULL && PF_RTABLEID_IS_VALID(r->rtableid)) {
		pf_mtag->pftag_rtableid = r->rtableid;
	}

#if PF_ECN
	/* add hints for ecn */
	pf_mtag->pftag_hdr = mtod(m, struct ip *);
	/* record address family */
	pf_mtag->pftag_flags &= ~(PF_TAG_HDR_INET | PF_TAG_HDR_INET6);
	switch (af) {
#if INET
	case AF_INET:
		pf_mtag->pftag_flags |= PF_TAG_HDR_INET;
		break;
#endif /* INET */
	case AF_INET6:
		pf_mtag->pftag_flags |= PF_TAG_HDR_INET6;
		break;
	}
#endif /* PF_ECN */

	/* indicate this is TCP */
	m->m_pkthdr.pkt_proto = IPPROTO_TCP;

	/* Make sure headers are 32-bit aligned */
	m->m_data += max_linkhdr;
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = NULL;
	bzero(m_mtod_current(m), len);
	switch (af) {
#if INET
	case AF_INET:
		h = mtod(m, struct ip *);

		/* IP header fields included in the TCP checksum */
		h->ip_p = IPPROTO_TCP;
		h->ip_len = htons(tlen);
		h->ip_src.s_addr = saddr->v4addr.s_addr;
		h->ip_dst.s_addr = daddr->v4addr.s_addr;

		th = (struct tcphdr *)(void *)((caddr_t)h + sizeof(struct ip));
		break;
#endif /* INET */
	case AF_INET6:
		h6 = mtod(m, struct ip6_hdr *);

		/* IP header fields included in the TCP checksum */
		h6->ip6_nxt = IPPROTO_TCP;
		h6->ip6_plen = htons(tlen);
		memcpy((void *)&h6->ip6_src, &saddr->v6addr, sizeof(struct in6_addr));
		memcpy((void *)&h6->ip6_dst, &daddr->v6addr, sizeof(struct in6_addr));

		th = (struct tcphdr *)(void *)
		    ((caddr_t)h6 + sizeof(struct ip6_hdr));
		break;
	}

	/* TCP header */
	th->th_sport = sport;
	th->th_dport = dport;
	th->th_seq = htonl(seq);
	th->th_ack = htonl(ack);
	th->th_off = tlen >> 2;
	th->th_flags = flags;
	th->th_win = htons(win);

	if (mss) {
		opt = (char *)(th + 1);
		opt[0] = TCPOPT_MAXSEG;
		opt[1] = 4;
#if BYTE_ORDER != BIG_ENDIAN
		HTONS(mss);
#endif
		bcopy((caddr_t)&mss, (caddr_t)(opt + 2), 2);
	}

	switch (af) {
#if INET
	case AF_INET: {
		struct route ro;

		/* TCP checksum */
		th->th_sum = in_cksum(m, len);

		/* Finish the IP header */
		h->ip_v = 4;
		h->ip_hl = sizeof(*h) >> 2;
		h->ip_tos = IPTOS_LOWDELAY;
		/*
		 * ip_output() expects ip_len and ip_off to be in host order.
		 */
		h->ip_len = len;
		h->ip_off = (path_mtu_discovery ? IP_DF : 0);
		h->ip_ttl = ttl ? ttl : ip_defttl;
		h->ip_sum = 0;

		bzero(&ro, sizeof(ro));
		ip_output(m, NULL, &ro, 0, NULL, NULL);
		ROUTE_RELEASE(&ro);
		break;
	}
#endif /* INET */
	case AF_INET6: {
		struct route_in6 ro6;

		/* TCP checksum */
		th->th_sum = in6_cksum(m, IPPROTO_TCP,
		    sizeof(struct ip6_hdr), tlen);

		h6->ip6_vfc |= IPV6_VERSION;
		h6->ip6_hlim = IPV6_DEFHLIM;

		ip6_output_setsrcifscope(m, IFSCOPE_UNKNOWN, NULL);
		ip6_output_setdstifscope(m, IFSCOPE_UNKNOWN, NULL);
		bzero(&ro6, sizeof(ro6));
		ip6_output(m, NULL, &ro6, 0, NULL, NULL, NULL);
		ROUTE_RELEASE(&ro6);
		break;
	}
	}
}

static __attribute__((noinline)) void
pf_send_icmp(pbuf_t *pbuf, u_int8_t type, u_int8_t code, sa_family_t af,
    struct pf_rule *r)
{
	struct mbuf     *m0;
	struct pf_mtag  *pf_mtag;

	m0 = pbuf_clone_to_mbuf(pbuf);
	if (m0 == NULL) {
		return;
	}

	if ((pf_mtag = pf_get_mtag(m0)) == NULL) {
		return;
	}

	pf_mtag->pftag_flags |= PF_TAG_GENERATED;

	if (PF_RTABLEID_IS_VALID(r->rtableid)) {
		pf_mtag->pftag_rtableid = r->rtableid;
	}

#if PF_ECN
	/* add hints for ecn */
	pf_mtag->pftag_hdr = mtod(m0, struct ip *);
	/* record address family */
	pf_mtag->pftag_flags &= ~(PF_TAG_HDR_INET | PF_TAG_HDR_INET6);
	switch (af) {
#if INET
	case AF_INET:
		pf_mtag->pftag_flags |= PF_TAG_HDR_INET;
		m0->m_pkthdr.pkt_proto = IPPROTO_ICMP;
		break;
#endif /* INET */
	case AF_INET6:
		pf_mtag->pftag_flags |= PF_TAG_HDR_INET6;
		m0->m_pkthdr.pkt_proto = IPPROTO_ICMPV6;
		break;
	}
#endif /* PF_ECN */

	switch (af) {
#if INET
	case AF_INET:
		icmp_error(m0, type, code, 0, 0);
		break;
#endif /* INET */
	case AF_INET6:
		icmp6_error(m0, type, code, 0);
		break;
	}
}

/*
 * Return 1 if the addresses a and b match (with mask m), otherwise return 0.
 * If n is 0, they match if they are equal. If n is != 0, they match if they
 * are different.
 */
int
pf_match_addr(u_int8_t n, struct pf_addr *a, struct pf_addr *m,
    struct pf_addr *b, sa_family_t af)
{
	int     match = 0;

	switch (af) {
#if INET
	case AF_INET:
		if ((a->addr32[0] & m->addr32[0]) ==
		    (b->addr32[0] & m->addr32[0])) {
			match++;
		}
		break;
#endif /* INET */
	case AF_INET6:
		if (((a->addr32[0] & m->addr32[0]) ==
		    (b->addr32[0] & m->addr32[0])) &&
		    ((a->addr32[1] & m->addr32[1]) ==
		    (b->addr32[1] & m->addr32[1])) &&
		    ((a->addr32[2] & m->addr32[2]) ==
		    (b->addr32[2] & m->addr32[2])) &&
		    ((a->addr32[3] & m->addr32[3]) ==
		    (b->addr32[3] & m->addr32[3]))) {
			match++;
		}
		break;
	}
	if (match) {
		if (n) {
			return 0;
		} else {
			return 1;
		}
	} else {
		if (n) {
			return 1;
		} else {
			return 0;
		}
	}
}

/*
 * Return 1 if b <= a <= e, otherwise return 0.
 */
int
pf_match_addr_range(struct pf_addr *b, struct pf_addr *e,
    struct pf_addr *a, sa_family_t af)
{
	switch (af) {
#if INET
	case AF_INET:
		if ((a->addr32[0] < b->addr32[0]) ||
		    (a->addr32[0] > e->addr32[0])) {
			return 0;
		}
		break;
#endif /* INET */
	case AF_INET6: {
		int     i;

		/* check a >= b */
		for (i = 0; i < 4; ++i) {
			if (a->addr32[i] > b->addr32[i]) {
				break;
			} else if (a->addr32[i] < b->addr32[i]) {
				return 0;
			}
		}
		/* check a <= e */
		for (i = 0; i < 4; ++i) {
			if (a->addr32[i] < e->addr32[i]) {
				break;
			} else if (a->addr32[i] > e->addr32[i]) {
				return 0;
			}
		}
		break;
	}
	}
	return 1;
}

int
pf_match(u_int8_t op, u_int32_t a1, u_int32_t a2, u_int32_t p)
{
	switch (op) {
	case PF_OP_IRG:
		return (p > a1) && (p < a2);
	case PF_OP_XRG:
		return (p < a1) || (p > a2);
	case PF_OP_RRG:
		return (p >= a1) && (p <= a2);
	case PF_OP_EQ:
		return p == a1;
	case PF_OP_NE:
		return p != a1;
	case PF_OP_LT:
		return p < a1;
	case PF_OP_LE:
		return p <= a1;
	case PF_OP_GT:
		return p > a1;
	case PF_OP_GE:
		return p >= a1;
	}
	return 0; /* never reached */
}

int
pf_match_port(u_int8_t op, u_int16_t a1, u_int16_t a2, u_int16_t p)
{
#if BYTE_ORDER != BIG_ENDIAN
	NTOHS(a1);
	NTOHS(a2);
	NTOHS(p);
#endif
	return pf_match(op, a1, a2, p);
}

int
pf_match_xport(u_int8_t proto, u_int8_t proto_variant, union pf_rule_xport *rx,
    union pf_state_xport *sx)
{
	int d = !0;

	if (sx) {
		switch (proto) {
		case IPPROTO_GRE:
			if (proto_variant == PF_GRE_PPTP_VARIANT) {
				d = (rx->call_id == sx->call_id);
			}
			break;

		case IPPROTO_ESP:
			d = (rx->spi == sx->spi);
			break;

		case IPPROTO_TCP:
		case IPPROTO_UDP:
		case IPPROTO_ICMP:
		case IPPROTO_ICMPV6:
			if (rx->range.op) {
				d = pf_match_port(rx->range.op,
				    rx->range.port[0], rx->range.port[1],
				    sx->port);
			}
			break;

		default:
			break;
		}
	}

	return d;
}

int
pf_match_uid(u_int8_t op, uid_t a1, uid_t a2, uid_t u)
{
	if (u == UID_MAX && op != PF_OP_EQ && op != PF_OP_NE) {
		return 0;
	}
	return pf_match(op, a1, a2, u);
}

int
pf_match_gid(u_int8_t op, gid_t a1, gid_t a2, gid_t g)
{
	if (g == GID_MAX && op != PF_OP_EQ && op != PF_OP_NE) {
		return 0;
	}
	return pf_match(op, a1, a2, g);
}

static int
pf_match_tag(struct pf_rule *r, struct pf_mtag *pf_mtag,
    int *tag)
{
	if (*tag == -1) {
		*tag = pf_mtag->pftag_tag;
	}

	return (!r->match_tag_not && r->match_tag == *tag) ||
	       (r->match_tag_not && r->match_tag != *tag);
}

int
pf_tag_packet(pbuf_t *pbuf, struct pf_mtag *pf_mtag, int tag,
    unsigned int rtableid, struct pf_pdesc *pd)
{
	if (tag <= 0 && !PF_RTABLEID_IS_VALID(rtableid) &&
	    (pd == NULL || !(pd->pktflags & PKTF_FLOW_ID))) {
		return 0;
	}

	if (pf_mtag == NULL && (pf_mtag = pf_get_mtag_pbuf(pbuf)) == NULL) {
		return 1;
	}

	if (tag > 0) {
		pf_mtag->pftag_tag = tag;
	}
	if (PF_RTABLEID_IS_VALID(rtableid)) {
		pf_mtag->pftag_rtableid = rtableid;
	}
	if (pd != NULL && (pd->pktflags & PKTF_FLOW_ID)) {
		*pbuf->pb_flowsrc = pd->flowsrc;
		*pbuf->pb_flowid = pd->flowhash;
		*pbuf->pb_flags |= pd->pktflags;
		*pbuf->pb_proto = pd->proto;
	}

	return 0;
}

void
pf_step_into_anchor(int *depth, struct pf_ruleset **rs, int n,
    struct pf_rule **r, struct pf_rule **a, int *match)
{
	struct pf_anchor_stackframe     *f;

	(*r)->anchor->match = 0;
	if (match) {
		*match = 0;
	}
	if (*depth >= (int)sizeof(pf_anchor_stack) /
	    (int)sizeof(pf_anchor_stack[0])) {
		printf("pf_step_into_anchor: stack overflow\n");
		*r = TAILQ_NEXT(*r, entries);
		return;
	} else if (*depth == 0 && a != NULL) {
		*a = *r;
	}
	f = pf_anchor_stack + (*depth)++;
	f->rs = *rs;
	f->r = *r;
	if ((*r)->anchor_wildcard) {
		f->parent = &(*r)->anchor->children;
		if ((f->child = RB_MIN(pf_anchor_node, f->parent)) ==
		    NULL) {
			*r = NULL;
			return;
		}
		*rs = &f->child->ruleset;
	} else {
		f->parent = NULL;
		f->child = NULL;
		*rs = &(*r)->anchor->ruleset;
	}
	*r = TAILQ_FIRST((*rs)->rules[n].active.ptr);
}

int
pf_step_out_of_anchor(int *depth, struct pf_ruleset **rs, int n,
    struct pf_rule **r, struct pf_rule **a, int *match)
{
	struct pf_anchor_stackframe     *f;
	int quick = 0;

	do {
		if (*depth <= 0) {
			break;
		}
		f = pf_anchor_stack + *depth - 1;
		if (f->parent != NULL && f->child != NULL) {
			if (f->child->match ||
			    (match != NULL && *match)) {
				f->r->anchor->match = 1;
				if (match) {
					*match = 0;
				}
			}
			f->child = RB_NEXT(pf_anchor_node, f->parent, f->child);
			if (f->child != NULL) {
				*rs = &f->child->ruleset;
				*r = TAILQ_FIRST((*rs)->rules[n].active.ptr);
				if (*r == NULL) {
					continue;
				} else {
					break;
				}
			}
		}
		(*depth)--;
		if (*depth == 0 && a != NULL) {
			*a = NULL;
		}
		*rs = f->rs;
		if (f->r->anchor->match || (match != NULL && *match)) {
			quick = f->r->quick;
		}
		*r = TAILQ_NEXT(f->r, entries);
	} while (*r == NULL);

	return quick;
}

void
pf_poolmask(struct pf_addr *naddr, struct pf_addr *raddr,
    struct pf_addr *rmask, struct pf_addr *saddr, sa_family_t af)
{
	switch (af) {
#if INET
	case AF_INET:
		naddr->addr32[0] = (raddr->addr32[0] & rmask->addr32[0]) |
		    ((rmask->addr32[0] ^ 0xffffffff) & saddr->addr32[0]);
		break;
#endif /* INET */
	case AF_INET6:
		naddr->addr32[0] = (raddr->addr32[0] & rmask->addr32[0]) |
		    ((rmask->addr32[0] ^ 0xffffffff) & saddr->addr32[0]);
		naddr->addr32[1] = (raddr->addr32[1] & rmask->addr32[1]) |
		    ((rmask->addr32[1] ^ 0xffffffff) & saddr->addr32[1]);
		naddr->addr32[2] = (raddr->addr32[2] & rmask->addr32[2]) |
		    ((rmask->addr32[2] ^ 0xffffffff) & saddr->addr32[2]);
		naddr->addr32[3] = (raddr->addr32[3] & rmask->addr32[3]) |
		    ((rmask->addr32[3] ^ 0xffffffff) & saddr->addr32[3]);
		break;
	}
}

void
pf_addr_inc(struct pf_addr *addr, sa_family_t af)
{
	switch (af) {
#if INET
	case AF_INET:
		addr->addr32[0] = htonl(ntohl(addr->addr32[0]) + 1);
		break;
#endif /* INET */
	case AF_INET6:
		if (addr->addr32[3] == 0xffffffff) {
			addr->addr32[3] = 0;
			if (addr->addr32[2] == 0xffffffff) {
				addr->addr32[2] = 0;
				if (addr->addr32[1] == 0xffffffff) {
					addr->addr32[1] = 0;
					addr->addr32[0] =
					    htonl(ntohl(addr->addr32[0]) + 1);
				} else {
					addr->addr32[1] =
					    htonl(ntohl(addr->addr32[1]) + 1);
				}
			} else {
				addr->addr32[2] =
				    htonl(ntohl(addr->addr32[2]) + 1);
			}
		} else {
			addr->addr32[3] =
			    htonl(ntohl(addr->addr32[3]) + 1);
		}
		break;
	}
}

#define mix(a, b, c) \
	do {                                    \
	        a -= b; a -= c; a ^= (c >> 13); \
	        b -= c; b -= a; b ^= (a << 8);  \
	        c -= a; c -= b; c ^= (b >> 13); \
	        a -= b; a -= c; a ^= (c >> 12); \
	        b -= c; b -= a; b ^= (a << 16); \
	        c -= a; c -= b; c ^= (b >> 5);  \
	        a -= b; a -= c; a ^= (c >> 3);  \
	        b -= c; b -= a; b ^= (a << 10); \
	        c -= a; c -= b; c ^= (b >> 15); \
	} while (0)

/*
 * hash function based on bridge_hash in if_bridge.c
 */
static void
pf_hash(struct pf_addr *inaddr, struct pf_addr *hash,
    struct pf_poolhashkey *key, sa_family_t af)
{
	u_int32_t       a = 0x9e3779b9, b = 0x9e3779b9, c = key->key32[0];

	switch (af) {
#if INET
	case AF_INET:
		a += inaddr->addr32[0];
		b += key->key32[1];
		mix(a, b, c);
		hash->addr32[0] = c + key->key32[2];
		break;
#endif /* INET */
	case AF_INET6:
		a += inaddr->addr32[0];
		b += inaddr->addr32[2];
		mix(a, b, c);
		hash->addr32[0] = c;
		a += inaddr->addr32[1];
		b += inaddr->addr32[3];
		c += key->key32[1];
		mix(a, b, c);
		hash->addr32[1] = c;
		a += inaddr->addr32[2];
		b += inaddr->addr32[1];
		c += key->key32[2];
		mix(a, b, c);
		hash->addr32[2] = c;
		a += inaddr->addr32[3];
		b += inaddr->addr32[0];
		c += key->key32[3];
		mix(a, b, c);
		hash->addr32[3] = c;
		break;
	}
}

static __attribute__((noinline)) int
pf_map_addr(sa_family_t af, struct pf_rule *r, struct pf_addr *saddr,
    struct pf_addr *naddr, struct pf_addr *init_addr, struct pf_src_node **sn)
{
	unsigned char            hash[16];
	struct pf_pool          *__single rpool = &r->rpool;
	struct pf_addr          *__single raddr = &rpool->cur->addr.v.a.addr;
	struct pf_addr          *__single rmask = &rpool->cur->addr.v.a.mask;
	struct pf_pooladdr      *__single acur = rpool->cur;
	struct pf_src_node       k;

	if (*sn == NULL && r->rpool.opts & PF_POOL_STICKYADDR &&
	    (r->rpool.opts & PF_POOL_TYPEMASK) != PF_POOL_NONE) {
		k.af = af;
		PF_ACPY(&k.addr, saddr, af);
		if (r->rule_flag & PFRULE_RULESRCTRACK ||
		    r->rpool.opts & PF_POOL_STICKYADDR) {
			k.rule.ptr = r;
		} else {
			k.rule.ptr = NULL;
		}
		pf_status.scounters[SCNT_SRC_NODE_SEARCH]++;
		*sn = RB_FIND(pf_src_tree, &tree_src_tracking, &k);
		if (*sn != NULL && !PF_AZERO(&(*sn)->raddr, rpool->af)) {
			PF_ACPY(naddr, &(*sn)->raddr, rpool->af);
			if (pf_status.debug >= PF_DEBUG_MISC) {
				printf("pf_map_addr: src tracking maps ");
				pf_print_host(&k.addr, 0, af);
				printf(" to ");
				pf_print_host(naddr, 0, rpool->af);
				printf("\n");
			}
			return 0;
		}
	}

	if (rpool->cur->addr.type == PF_ADDR_NOROUTE) {
		return 1;
	}
	if (rpool->cur->addr.type == PF_ADDR_DYNIFTL) {
		if (rpool->cur->addr.p.dyn == NULL) {
			return 1;
		}
		switch (rpool->af) {
#if INET
		case AF_INET:
			if (rpool->cur->addr.p.dyn->pfid_acnt4 < 1 &&
			    (rpool->opts & PF_POOL_TYPEMASK) !=
			    PF_POOL_ROUNDROBIN) {
				return 1;
			}
			raddr = &rpool->cur->addr.p.dyn->pfid_addr4;
			rmask = &rpool->cur->addr.p.dyn->pfid_mask4;
			break;
#endif /* INET */
		case AF_INET6:
			if (rpool->cur->addr.p.dyn->pfid_acnt6 < 1 &&
			    (rpool->opts & PF_POOL_TYPEMASK) !=
			    PF_POOL_ROUNDROBIN) {
				return 1;
			}
			raddr = &rpool->cur->addr.p.dyn->pfid_addr6;
			rmask = &rpool->cur->addr.p.dyn->pfid_mask6;
			break;
		}
	} else if (rpool->cur->addr.type == PF_ADDR_TABLE) {
		if ((rpool->opts & PF_POOL_TYPEMASK) != PF_POOL_ROUNDROBIN) {
			return 1; /* unsupported */
		}
	} else {
		raddr = &rpool->cur->addr.v.a.addr;
		rmask = &rpool->cur->addr.v.a.mask;
	}

	switch (rpool->opts & PF_POOL_TYPEMASK) {
	case PF_POOL_NONE:
		PF_ACPY(naddr, raddr, rpool->af);
		break;
	case PF_POOL_BITMASK:
		ASSERT(af == rpool->af);
		PF_POOLMASK(naddr, raddr, rmask, saddr, af);
		break;
	case PF_POOL_RANDOM:
		if (init_addr != NULL && PF_AZERO(init_addr, rpool->af)) {
			switch (af) {
#if INET
			case AF_INET:
				rpool->counter.addr32[0] = htonl(random());
				break;
#endif /* INET */
			case AF_INET6:
				if (rmask->addr32[3] != 0xffffffff) {
					rpool->counter.addr32[3] =
					    RandomULong();
				} else {
					break;
				}
				if (rmask->addr32[2] != 0xffffffff) {
					rpool->counter.addr32[2] =
					    RandomULong();
				} else {
					break;
				}
				if (rmask->addr32[1] != 0xffffffff) {
					rpool->counter.addr32[1] =
					    RandomULong();
				} else {
					break;
				}
				if (rmask->addr32[0] != 0xffffffff) {
					rpool->counter.addr32[0] =
					    RandomULong();
				}
				break;
			}
			PF_POOLMASK(naddr, raddr, rmask, &rpool->counter,
			    rpool->af);
			PF_ACPY(init_addr, naddr, rpool->af);
		} else {
			PF_AINC(&rpool->counter, rpool->af);
			PF_POOLMASK(naddr, raddr, rmask, &rpool->counter,
			    rpool->af);
		}
		break;
	case PF_POOL_SRCHASH:
		ASSERT(af == rpool->af);
		PF_POOLMASK(naddr, raddr, rmask, saddr, af);
		pf_hash(saddr, (struct pf_addr *)(void *)&hash,
		    &rpool->key, af);
		PF_POOLMASK(naddr, raddr, rmask,
		    (struct pf_addr *)(void *)&hash, af);
		break;
	case PF_POOL_ROUNDROBIN:
		if (rpool->cur->addr.type == PF_ADDR_TABLE) {
			if (!pfr_pool_get(rpool->cur->addr.p.tbl,
			    &rpool->tblidx, &rpool->counter,
			    &raddr, &rmask, rpool->af)) {
				goto get_addr;
			}
		} else if (rpool->cur->addr.type == PF_ADDR_DYNIFTL) {
			if (rpool->cur->addr.p.dyn != NULL &&
			    !pfr_pool_get(rpool->cur->addr.p.dyn->pfid_kt,
			    &rpool->tblidx, &rpool->counter,
			    &raddr, &rmask, af)) {
				goto get_addr;
			}
		} else if (pf_match_addr(0, raddr, rmask, &rpool->counter,
		    rpool->af)) {
			goto get_addr;
		}

try_next:
		if ((rpool->cur = TAILQ_NEXT(rpool->cur, entries)) == NULL) {
			rpool->cur = TAILQ_FIRST(&rpool->list);
		}
		if (rpool->cur->addr.type == PF_ADDR_TABLE) {
			rpool->tblidx = -1;
			if (pfr_pool_get(rpool->cur->addr.p.tbl,
			    &rpool->tblidx, &rpool->counter,
			    &raddr, &rmask, rpool->af)) {
				/* table contains no address of type
				 * 'rpool->af' */
				if (rpool->cur != acur) {
					goto try_next;
				}
				return 1;
			}
		} else if (rpool->cur->addr.type == PF_ADDR_DYNIFTL) {
			rpool->tblidx = -1;
			if (rpool->cur->addr.p.dyn == NULL) {
				return 1;
			}
			if (pfr_pool_get(rpool->cur->addr.p.dyn->pfid_kt,
			    &rpool->tblidx, &rpool->counter,
			    &raddr, &rmask, rpool->af)) {
				/* table contains no address of type
				 * 'rpool->af' */
				if (rpool->cur != acur) {
					goto try_next;
				}
				return 1;
			}
		} else {
			raddr = &rpool->cur->addr.v.a.addr;
			rmask = &rpool->cur->addr.v.a.mask;
			PF_ACPY(&rpool->counter, raddr, rpool->af);
		}

get_addr:
		PF_ACPY(naddr, &rpool->counter, rpool->af);
		if (init_addr != NULL && PF_AZERO(init_addr, rpool->af)) {
			PF_ACPY(init_addr, naddr, rpool->af);
		}
		PF_AINC(&rpool->counter, rpool->af);
		break;
	}
	if (*sn != NULL) {
		PF_ACPY(&(*sn)->raddr, naddr, rpool->af);
	}

	if (pf_status.debug >= PF_DEBUG_MISC &&
	    (rpool->opts & PF_POOL_TYPEMASK) != PF_POOL_NONE) {
		printf("pf_map_addr: selected address ");
		pf_print_host(naddr, 0, rpool->af);
		printf("\n");
	}

	return 0;
}

static __attribute__((noinline)) int
pf_get_sport(struct pf_pdesc *pd, struct pfi_kif *kif, struct pf_rule *r,
    struct pf_addr *saddr, union pf_state_xport *sxport, struct pf_addr *daddr,
    union pf_state_xport *dxport, struct pf_addr *naddr,
    union pf_state_xport *nxport, struct pf_src_node **sn
#if SKYWALK
    , netns_token *pnstoken
#endif
    )
{
#pragma unused(kif)
	struct pf_state_key_cmp key;
	struct pf_addr          init_addr;
	unsigned int cut;
	sa_family_t af = pd->af;
	u_int8_t proto = pd->proto;
	unsigned int low = r->rpool.proxy_port[0];
	unsigned int high = r->rpool.proxy_port[1];

	bzero(&init_addr, sizeof(init_addr));
	if (pf_map_addr(af, r, saddr, naddr, &init_addr, sn)) {
		return 1;
	}

	if (proto == IPPROTO_ICMP) {
		low = 1;
		high = 65535;
	}

	if (!nxport) {
		return 0; /* No output necessary. */
	}
	/*--- Special mapping rules for UDP ---*/
	if (proto == IPPROTO_UDP) {
		/*--- Never float IKE source port ---*/
		if (ntohs(sxport->port) == PF_IKE_PORT) {
			nxport->port = sxport->port;
			return 0;
		}

		/*--- Apply exterior mapping options ---*/
		if (r->extmap > PF_EXTMAP_APD) {
			struct pf_state *s;

			TAILQ_FOREACH(s, &state_list, entry_list) {
				struct pf_state_key *sk = s->state_key;
				if (!sk) {
					continue;
				}
				if (s->nat_rule.ptr != r) {
					continue;
				}
				if (sk->proto != IPPROTO_UDP ||
				    sk->af_lan != af) {
					continue;
				}
				if (sk->lan.xport.port != sxport->port) {
					continue;
				}
				if (PF_ANEQ(&sk->lan.addr, saddr, af)) {
					continue;
				}
				if (r->extmap < PF_EXTMAP_EI &&
				    PF_ANEQ(&sk->ext_lan.addr, daddr, af)) {
					continue;
				}

#if SKYWALK
				if (netns_reserve(pnstoken, naddr->addr32,
				    NETNS_AF_SIZE(af), proto, sxport->port,
				    NETNS_PF, NULL) != 0) {
					return 1;
				}
#endif
				nxport->port = sk->gwy.xport.port;
				return 0;
			}
		}
	} else if (proto == IPPROTO_TCP) {
		struct pf_state* s;
		/*
		 * APPLE MODIFICATION: <rdar://problem/6546358>
		 * Fix allows....NAT to use a single binding for TCP session
		 * with same source IP and source port
		 */
		TAILQ_FOREACH(s, &state_list, entry_list) {
			struct pf_state_key* sk = s->state_key;
			if (!sk) {
				continue;
			}
			if (s->nat_rule.ptr != r) {
				continue;
			}
			if (sk->proto != IPPROTO_TCP || sk->af_lan != af) {
				continue;
			}
			if (sk->lan.xport.port != sxport->port) {
				continue;
			}
			if (!(PF_AEQ(&sk->lan.addr, saddr, af))) {
				continue;
			}
#if SKYWALK
			if (netns_reserve(pnstoken, naddr->addr32,
			    NETNS_AF_SIZE(af), proto, sxport->port,
			    NETNS_PF, NULL) != 0) {
				return 1;
			}
#endif
			nxport->port = sk->gwy.xport.port;
			return 0;
		}
	}
	do {
		key.af_gwy = af;
		key.proto = proto;
		PF_ACPY(&key.ext_gwy.addr, daddr, key.af_gwy);
		PF_ACPY(&key.gwy.addr, naddr, key.af_gwy);
		switch (proto) {
		case IPPROTO_UDP:
			key.proto_variant = r->extfilter;
			break;
		default:
			key.proto_variant = 0;
			break;
		}
		if (dxport) {
			key.ext_gwy.xport = *dxport;
		} else {
			memset(&key.ext_gwy.xport, 0,
			    sizeof(key.ext_gwy.xport));
		}
		/*
		 * port search; start random, step;
		 * similar 2 portloop in in_pcbbind
		 */
		if (!(proto == IPPROTO_TCP || proto == IPPROTO_UDP ||
		    proto == IPPROTO_ICMP)) {
			if (dxport) {
				key.gwy.xport = *dxport;
			} else {
				memset(&key.gwy.xport, 0,
				    sizeof(key.gwy.xport));
			}
#if SKYWALK
			/* Nothing to do: netns handles TCP/UDP only */
#endif
			if (pf_find_state_all(&key, PF_IN, NULL) == NULL) {
				return 0;
			}
		} else if (low == 0 && high == 0) {
			key.gwy.xport = *nxport;
			if (pf_find_state_all(&key, PF_IN, NULL) == NULL
#if SKYWALK
			    && ((proto != IPPROTO_TCP && proto != IPPROTO_UDP)
			    || netns_reserve(pnstoken, naddr->addr32,
			    NETNS_AF_SIZE(af), proto, nxport->port,
			    NETNS_PF, NULL) == 0)
#endif
			    ) {
				return 0;
			}
		} else if (low == high) {
			key.gwy.xport.port = htons(low);
			if (pf_find_state_all(&key, PF_IN, NULL) == NULL
#if SKYWALK
			    && ((proto != IPPROTO_TCP && proto != IPPROTO_UDP)
			    || netns_reserve(pnstoken, naddr->addr32,
			    NETNS_AF_SIZE(af), proto, htons(low),
			    NETNS_PF, NULL) == 0)
#endif
			    ) {
				nxport->port = htons(low);
				return 0;
			}
		} else {
			unsigned int tmp;
			if (low > high) {
				tmp = low;
				low = high;
				high = tmp;
			}
			/* low < high */
			cut = htonl(random()) % (1 + high - low) + low;
			/* low <= cut <= high */
			for (tmp = cut; tmp <= high; ++(tmp)) {
				key.gwy.xport.port = htons(tmp);
				if (pf_find_state_all(&key, PF_IN, NULL) == NULL
#if SKYWALK
				    && ((proto != IPPROTO_TCP && proto != IPPROTO_UDP)
				    || netns_reserve(pnstoken, naddr->addr32,
				    NETNS_AF_SIZE(af), proto, htons(tmp),
				    NETNS_PF, NULL) == 0)
#endif
				    ) {
					nxport->port = htons(tmp);
					return 0;
				}
			}
			for (tmp = cut - 1; tmp >= low; --(tmp)) {
				key.gwy.xport.port = htons(tmp);
				if (pf_find_state_all(&key, PF_IN, NULL) == NULL
#if SKYWALK
				    && ((proto != IPPROTO_TCP && proto != IPPROTO_UDP)
				    || netns_reserve(pnstoken, naddr->addr32,
				    NETNS_AF_SIZE(af), proto, htons(tmp),
				    NETNS_PF, NULL) == 0)
#endif
				    ) {
					nxport->port = htons(tmp);
					return 0;
				}
			}
		}

		switch (r->rpool.opts & PF_POOL_TYPEMASK) {
		case PF_POOL_RANDOM:
		case PF_POOL_ROUNDROBIN:
			if (pf_map_addr(af, r, saddr, naddr, &init_addr, sn)) {
				return 1;
			}
			break;
		case PF_POOL_NONE:
		case PF_POOL_SRCHASH:
		case PF_POOL_BITMASK:
		default:
			return 1;
		}
	} while (!PF_AEQ(&init_addr, naddr, af));

	return 1;                                     /* none available */
}

static __attribute__((noinline)) struct pf_rule *
pf_match_translation(struct pf_pdesc *pd, pbuf_t *pbuf, int off,
    int direction, struct pfi_kif *kif, struct pf_addr *saddr,
    union pf_state_xport *sxport, struct pf_addr *daddr,
    union pf_state_xport *dxport, int rs_num)
{
	struct pf_rule          *__single r, *__single rm = NULL;
	struct pf_ruleset       *__single ruleset = NULL;
	int                      tag = -1;
	unsigned int             rtableid = IFSCOPE_NONE;
	int                      asd = 0;

	r = TAILQ_FIRST(pf_main_ruleset.rules[rs_num].active.ptr);
	while (r && rm == NULL) {
		struct pf_rule_addr     *src = NULL, *dst = NULL;
		struct pf_addr_wrap     *xdst = NULL;
		struct pf_addr_wrap     *xsrc = NULL;
		union pf_rule_xport     rdrxport;

		if (r->action == PF_BINAT && direction == PF_IN) {
			src = &r->dst;
			if (r->rpool.cur != NULL) {
				xdst = &r->rpool.cur->addr;
			}
		} else if (r->action == PF_RDR && direction == PF_OUT) {
			dst = &r->src;
			src = &r->dst;
			if (r->rpool.cur != NULL) {
				rdrxport.range.op = PF_OP_EQ;
				rdrxport.range.port[0] =
				    htons(r->rpool.proxy_port[0]);
				xsrc = &r->rpool.cur->addr;
			}
		} else {
			src = &r->src;
			dst = &r->dst;
		}

		r->evaluations++;
		if (pfi_kif_match(r->kif, kif) == r->ifnot) {
			r = r->skip[PF_SKIP_IFP].ptr;
		} else if (r->direction && r->direction != direction) {
			r = r->skip[PF_SKIP_DIR].ptr;
		} else if (r->af && r->af != pd->af) {
			r = r->skip[PF_SKIP_AF].ptr;
		} else if (r->proto && r->proto != pd->proto) {
			r = r->skip[PF_SKIP_PROTO].ptr;
		} else if (xsrc && PF_MISMATCHAW(xsrc, saddr, pd->af, 0, NULL)) {
			r = TAILQ_NEXT(r, entries);
		} else if (!xsrc && PF_MISMATCHAW(&src->addr, saddr, pd->af,
		    src->neg, kif)) {
			r = TAILQ_NEXT(r, entries);
		} else if (xsrc && (!rdrxport.range.port[0] ||
		    !pf_match_xport(r->proto, r->proto_variant, &rdrxport,
		    sxport))) {
			r = TAILQ_NEXT(r, entries);
		} else if (!xsrc && !pf_match_xport(r->proto,
		    r->proto_variant, &src->xport, sxport)) {
			r = r->skip[src == &r->src ? PF_SKIP_SRC_PORT :
			    PF_SKIP_DST_PORT].ptr;
		} else if (dst != NULL &&
		    PF_MISMATCHAW(&dst->addr, daddr, pd->af, dst->neg, NULL)) {
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		} else if (xdst != NULL && PF_MISMATCHAW(xdst, daddr, pd->af,
		    0, NULL)) {
			r = TAILQ_NEXT(r, entries);
		} else if (dst && !pf_match_xport(r->proto, r->proto_variant,
		    &dst->xport, dxport)) {
			r = r->skip[PF_SKIP_DST_PORT].ptr;
		} else if (r->match_tag && !pf_match_tag(r, pd->pf_mtag, &tag)) {
			r = TAILQ_NEXT(r, entries);
		} else if (r->os_fingerprint != PF_OSFP_ANY && (pd->proto !=
		    IPPROTO_TCP || !pf_osfp_match(pf_osfp_fingerprint(pd, pbuf,
		    off, pf_pd_get_hdr_tcp(pd)), r->os_fingerprint))) {
			r = TAILQ_NEXT(r, entries);
		} else {
			if (r->tag) {
				tag = r->tag;
			}
			if (PF_RTABLEID_IS_VALID(r->rtableid)) {
				rtableid = r->rtableid;
			}
			if (r->anchor == NULL) {
				rm = r;
			} else {
				pf_step_into_anchor(&asd, &ruleset, rs_num,
				    &r, NULL, NULL);
			}
		}
		if (r == NULL) {
			pf_step_out_of_anchor(&asd, &ruleset, rs_num, &r,
			    NULL, NULL);
		}
	}
	if (pf_tag_packet(pbuf, pd->pf_mtag, tag, rtableid, NULL)) {
		return NULL;
	}
	if (rm != NULL && (rm->action == PF_NONAT ||
	    rm->action == PF_NORDR || rm->action == PF_NOBINAT ||
	    rm->action == PF_NONAT64)) {
		return NULL;
	}
	return rm;
}

/*
 * Get address translation information for NAT/BINAT/RDR
 * pd		: pf packet descriptor
 * pbuf		: pbuf holding the packet
 * off		: offset to protocol header
 * direction	: direction of packet
 * kif		: pf interface info obtained from the packet's recv interface
 * sn		: source node pointer (output)
 * saddr	: packet source address
 * sxport	: packet source port
 * daddr	: packet destination address
 * dxport	: packet destination port
 * nsxport	: translated source port (output)
 *
 * Translated source & destination address are updated in pd->nsaddr &
 * pd->ndaddr
 */
static __attribute__((noinline)) struct pf_rule *
pf_get_translation_aux(struct pf_pdesc *pd, pbuf_t *pbuf, int off,
    int direction, struct pfi_kif *kif, struct pf_src_node **sn,
    struct pf_addr *saddr, union pf_state_xport *sxport, struct pf_addr *daddr,
    union pf_state_xport *dxport, union pf_state_xport *nsxport
#if SKYWALK
    , netns_token *pnstoken
#endif
    )
{
	struct pf_rule  *r = NULL;
	pd->naf = pd->af;

	if (direction == PF_OUT) {
		r = pf_match_translation(pd, pbuf, off, direction, kif, saddr,
		    sxport, daddr, dxport, PF_RULESET_BINAT);
		if (r == NULL) {
			r = pf_match_translation(pd, pbuf, off, direction, kif,
			    saddr, sxport, daddr, dxport, PF_RULESET_RDR);
		}
		if (r == NULL) {
			r = pf_match_translation(pd, pbuf, off, direction, kif,
			    saddr, sxport, daddr, dxport, PF_RULESET_NAT);
		}
	} else {
		r = pf_match_translation(pd, pbuf, off, direction, kif, saddr,
		    sxport, daddr, dxport, PF_RULESET_RDR);
		if (r == NULL) {
			r = pf_match_translation(pd, pbuf, off, direction, kif,
			    saddr, sxport, daddr, dxport, PF_RULESET_BINAT);
		}
	}

	if (r != NULL) {
		struct pf_addr *nsaddr = &pd->naddr;
		struct pf_addr *ndaddr = &pd->ndaddr;

		PF_ACPY(nsaddr, saddr, pd->af);
		PF_ACPY(ndaddr, daddr, pd->af);

		switch (r->action) {
		case PF_NONAT:
		case PF_NONAT64:
		case PF_NOBINAT:
		case PF_NORDR:
			return NULL;
		case PF_NAT:
		case PF_NAT64:
			/*
			 * we do NAT64 on incoming path and we call ip_input
			 * which asserts receive interface to be not NULL.
			 * The below check is to prevent NAT64 action on any
			 * packet generated by local entity using synthesized
			 * IPv6 address.
			 */
			if ((r->action == PF_NAT64) && (direction == PF_OUT)) {
				return NULL;
			}

			if (pf_get_sport(pd, kif, r, saddr, sxport, daddr,
			    dxport, nsaddr, nsxport, sn
#if SKYWALK
			    , pnstoken
#endif
			    )) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: NAT proxy port allocation "
				    "(%u-%u) failed\n",
				    r->rpool.proxy_port[0],
				    r->rpool.proxy_port[1]));
				return NULL;
			}
			/*
			 * For NAT64 the destination IPv4 address is derived
			 * from the last 32 bits of synthesized IPv6 address
			 */
			if (r->action == PF_NAT64) {
				ndaddr->v4addr.s_addr = daddr->addr32[3];
				pd->naf = AF_INET;
			}
			break;
		case PF_BINAT:
			switch (direction) {
			case PF_OUT:
				if (r->rpool.cur->addr.type ==
				    PF_ADDR_DYNIFTL) {
					if (r->rpool.cur->addr.p.dyn == NULL) {
						return NULL;
					}
					switch (pd->af) {
#if INET
					case AF_INET:
						if (r->rpool.cur->addr.p.dyn->
						    pfid_acnt4 < 1) {
							return NULL;
						}
						PF_POOLMASK(nsaddr,
						    &r->rpool.cur->addr.p.dyn->
						    pfid_addr4,
						    &r->rpool.cur->addr.p.dyn->
						    pfid_mask4,
						    saddr, AF_INET);
						break;
#endif /* INET */
					case AF_INET6:
						if (r->rpool.cur->addr.p.dyn->
						    pfid_acnt6 < 1) {
							return NULL;
						}
						PF_POOLMASK(nsaddr,
						    &r->rpool.cur->addr.p.dyn->
						    pfid_addr6,
						    &r->rpool.cur->addr.p.dyn->
						    pfid_mask6,
						    saddr, AF_INET6);
						break;
					}
				} else {
					PF_POOLMASK(nsaddr,
					    &r->rpool.cur->addr.v.a.addr,
					    &r->rpool.cur->addr.v.a.mask,
					    saddr, pd->af);
				}
				break;
			case PF_IN:
				if (r->src.addr.type == PF_ADDR_DYNIFTL) {
					if (r->src.addr.p.dyn == NULL) {
						return NULL;
					}
					switch (pd->af) {
#if INET
					case AF_INET:
						if (r->src.addr.p.dyn->
						    pfid_acnt4 < 1) {
							return NULL;
						}
						PF_POOLMASK(ndaddr,
						    &r->src.addr.p.dyn->
						    pfid_addr4,
						    &r->src.addr.p.dyn->
						    pfid_mask4,
						    daddr, AF_INET);
						break;
#endif /* INET */
					case AF_INET6:
						if (r->src.addr.p.dyn->
						    pfid_acnt6 < 1) {
							return NULL;
						}
						PF_POOLMASK(ndaddr,
						    &r->src.addr.p.dyn->
						    pfid_addr6,
						    &r->src.addr.p.dyn->
						    pfid_mask6,
						    daddr, AF_INET6);
						break;
					}
				} else {
					PF_POOLMASK(ndaddr,
					    &r->src.addr.v.a.addr,
					    &r->src.addr.v.a.mask, daddr,
					    pd->af);
				}
				break;
			}
			break;
		case PF_RDR: {
			switch (direction) {
			case PF_OUT:
				if (r->dst.addr.type == PF_ADDR_DYNIFTL) {
					if (r->dst.addr.p.dyn == NULL) {
						return NULL;
					}
					switch (pd->af) {
#if INET
					case AF_INET:
						if (r->dst.addr.p.dyn->
						    pfid_acnt4 < 1) {
							return NULL;
						}
						PF_POOLMASK(nsaddr,
						    &r->dst.addr.p.dyn->
						    pfid_addr4,
						    &r->dst.addr.p.dyn->
						    pfid_mask4,
						    daddr, AF_INET);
						break;
#endif /* INET */
					case AF_INET6:
						if (r->dst.addr.p.dyn->
						    pfid_acnt6 < 1) {
							return NULL;
						}
						PF_POOLMASK(nsaddr,
						    &r->dst.addr.p.dyn->
						    pfid_addr6,
						    &r->dst.addr.p.dyn->
						    pfid_mask6,
						    daddr, AF_INET6);
						break;
					}
				} else {
					PF_POOLMASK(nsaddr,
					    &r->dst.addr.v.a.addr,
					    &r->dst.addr.v.a.mask,
					    daddr, pd->af);
				}
				if (nsxport && r->dst.xport.range.port[0]) {
					nsxport->port =
					    r->dst.xport.range.port[0];
				}
				break;
			case PF_IN:
				if (pf_map_addr(pd->af, r, saddr,
				    ndaddr, NULL, sn)) {
					return NULL;
				}
				if ((r->rpool.opts & PF_POOL_TYPEMASK) ==
				    PF_POOL_BITMASK) {
					PF_POOLMASK(ndaddr, ndaddr,
					    &r->rpool.cur->addr.v.a.mask, daddr,
					    pd->af);
				}

				if (nsxport && dxport) {
					if (r->rpool.proxy_port[1]) {
						u_int32_t       tmp_nport;

						tmp_nport =
						    ((ntohs(dxport->port) -
						    ntohs(r->dst.xport.range.
						    port[0])) %
						    (r->rpool.proxy_port[1] -
						    r->rpool.proxy_port[0] +
						    1)) + r->rpool.proxy_port[0];

						/* wrap around if necessary */
						if (tmp_nport > 65535) {
							tmp_nport -= 65535;
						}
						nsxport->port =
						    htons((u_int16_t)tmp_nport);
					} else if (r->rpool.proxy_port[0]) {
						nsxport->port = htons(r->rpool.
						    proxy_port[0]);
					}
				}
				break;
			}
			break;
		}
		default:
			return NULL;
		}
	}

	return r;
}

int
pf_socket_lookup(int direction, struct pf_pdesc *pd)
{
	struct pf_addr          *__single saddr, *__single daddr;
	u_int16_t                sport, dport;
	struct inpcbinfo        *__single pi;
	int                     inp = 0;

	if (pd == NULL) {
		return -1;
	}
	pd->lookup.uid = UID_MAX;
	pd->lookup.gid = GID_MAX;
	pd->lookup.pid = NO_PID;

	switch (pd->proto) {
	case IPPROTO_TCP:
		if (pf_pd_get_hdr_tcp(pd) == NULL) {
			return -1;
		}
		sport = pf_pd_get_hdr_tcp(pd)->th_sport;
		dport = pf_pd_get_hdr_tcp(pd)->th_dport;
		pi = &tcbinfo;
		break;
	case IPPROTO_UDP:
		if (pf_pd_get_hdr_udp(pd) == NULL) {
			return -1;
		}
		sport = pf_pd_get_hdr_udp(pd)->uh_sport;
		dport = pf_pd_get_hdr_udp(pd)->uh_dport;
		pi = &udbinfo;
		break;
	default:
		return -1;
	}
	if (direction == PF_IN) {
		saddr = pd->src;
		daddr = pd->dst;
	} else {
		u_int16_t       p;

		p = sport;
		sport = dport;
		dport = p;
		saddr = pd->dst;
		daddr = pd->src;
	}
	switch (pd->af) {
#if INET
	case AF_INET:
		inp = in_pcblookup_hash_exists(pi, saddr->v4addr, sport, daddr->v4addr, dport,
		    0, &pd->lookup.uid, &pd->lookup.gid, NULL);
		if (inp == 0) {
			struct in6_addr s6, d6;

			memset(&s6, 0, sizeof(s6));
			s6.s6_addr16[5] = htons(0xffff);
			memcpy(&s6.s6_addr32[3], &saddr->v4addr,
			    sizeof(saddr->v4addr));

			memset(&d6, 0, sizeof(d6));
			d6.s6_addr16[5] = htons(0xffff);
			memcpy(&d6.s6_addr32[3], &daddr->v4addr,
			    sizeof(daddr->v4addr));

			inp = in6_pcblookup_hash_exists(pi, &s6, sport, IFSCOPE_NONE,
			    &d6, dport, IFSCOPE_NONE, 0, &pd->lookup.uid, &pd->lookup.gid, NULL, false);
			if (inp == 0) {
				inp = in_pcblookup_hash_exists(pi, saddr->v4addr, sport,
				    daddr->v4addr, dport, INPLOOKUP_WILDCARD, &pd->lookup.uid, &pd->lookup.gid, NULL);
				if (inp == 0) {
					inp = in6_pcblookup_hash_exists(pi, &s6, sport, IFSCOPE_NONE,
					    &d6, dport, IFSCOPE_NONE, INPLOOKUP_WILDCARD,
					    &pd->lookup.uid, &pd->lookup.gid, NULL, false);
					if (inp == 0) {
						return -1;
					}
				}
			}
		}
		break;
#endif /* INET */
	case AF_INET6:
		inp = in6_pcblookup_hash_exists(pi, &saddr->v6addr, sport, IFSCOPE_UNKNOWN, &daddr->v6addr,
		    dport, IFSCOPE_UNKNOWN, 0, &pd->lookup.uid, &pd->lookup.gid, NULL, false);
		if (inp == 0) {
			inp = in6_pcblookup_hash_exists(pi, &saddr->v6addr, sport, IFSCOPE_UNKNOWN,
			    &daddr->v6addr, dport, IFSCOPE_UNKNOWN, INPLOOKUP_WILDCARD,
			    &pd->lookup.uid, &pd->lookup.gid, NULL, false);
			if (inp == 0) {
				return -1;
			}
		}
		break;

	default:
		return -1;
	}

	return 1;
}

static __attribute__((noinline)) u_int8_t
pf_get_wscale(pbuf_t *pbuf, int off, u_int16_t th_off, sa_family_t af)
{
	int              hlen;
	u_int8_t         hdr[60];
	u_int8_t        *opt, optlen;
	u_int8_t         wscale = 0;

	hlen = th_off << 2;             /* hlen <= sizeof (hdr) */
	if (hlen <= (int)sizeof(struct tcphdr)) {
		return 0;
	}
	if (!pf_pull_hdr(pbuf, off, hdr, sizeof(hdr), hlen, NULL, NULL, af)) {
		return 0;
	}
	opt = hdr + sizeof(struct tcphdr);
	hlen -= sizeof(struct tcphdr);
	while (hlen >= 3) {
		switch (*opt) {
		case TCPOPT_EOL:
		case TCPOPT_NOP:
			++opt;
			--hlen;
			break;
		case TCPOPT_WINDOW:
			wscale = opt[2];
			if (wscale > TCP_MAX_WINSHIFT) {
				wscale = TCP_MAX_WINSHIFT;
			}
			wscale |= PF_WSCALE_FLAG;
			OS_FALLTHROUGH;
		default:
			optlen = opt[1];
			if (optlen < 2) {
				optlen = 2;
			}
			hlen -= optlen;
			opt += optlen;
			break;
		}
	}
	return wscale;
}

static __attribute__((noinline)) u_int16_t
pf_get_mss(pbuf_t *pbuf, int off, u_int16_t th_off, sa_family_t af)
{
	int              hlen;
	u_int8_t         hdr[60];
	u_int8_t        *opt, optlen;
	u_int16_t        mss = tcp_mssdflt;

	hlen = th_off << 2;     /* hlen <= sizeof (hdr) */
	if (hlen <= (int)sizeof(struct tcphdr)) {
		return 0;
	}
	if (!pf_pull_hdr(pbuf, off, hdr, sizeof(hdr), hlen, NULL, NULL, af)) {
		return 0;
	}
	opt = hdr + sizeof(struct tcphdr);
	hlen -= sizeof(struct tcphdr);
	while (hlen >= TCPOLEN_MAXSEG) {
		switch (*opt) {
		case TCPOPT_EOL:
		case TCPOPT_NOP:
			++opt;
			--hlen;
			break;
		case TCPOPT_MAXSEG:
			bcopy((caddr_t)(opt + 2), (caddr_t)&mss, 2);
#if BYTE_ORDER != BIG_ENDIAN
			NTOHS(mss);
#endif
			OS_FALLTHROUGH;
		default:
			optlen = opt[1];
			if (optlen < 2) {
				optlen = 2;
			}
			hlen -= optlen;
			opt += optlen;
			break;
		}
	}
	return mss;
}

static __attribute__((noinline)) u_int16_t
pf_calc_mss(struct pf_addr *addr, sa_family_t af, u_int16_t offer)
{
#if INET
	struct sockaddr_in      *dst;
	struct route             ro;
#endif /* INET */
	struct sockaddr_in6     *dst6;
	struct route_in6         ro6;
	struct rtentry          *rt = NULL;
	int                      hlen;
	u_int16_t                mss = tcp_mssdflt;

	switch (af) {
#if INET
	case AF_INET:
		hlen = sizeof(struct ip);
		bzero(&ro, sizeof(ro));
		dst = (struct sockaddr_in *)(void *)&ro.ro_dst;
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = addr->v4addr;
		rtalloc(&ro);
		rt = ro.ro_rt;
		break;
#endif /* INET */
	case AF_INET6:
		hlen = sizeof(struct ip6_hdr);
		bzero(&ro6, sizeof(ro6));
		dst6 = (struct sockaddr_in6 *)(void *)&ro6.ro_dst;
		dst6->sin6_family = AF_INET6;
		dst6->sin6_len = sizeof(*dst6);
		dst6->sin6_addr = addr->v6addr;
		rtalloc((struct route *)&ro);
		rt = ro6.ro_rt;
		break;
	default:
		panic("pf_calc_mss: not AF_INET or AF_INET6!");
		return 0;
	}

	if (rt && rt->rt_ifp) {
		/* This is relevant only for PF SYN Proxy */
		int interface_mtu = rt->rt_ifp->if_mtu;

		if (af == AF_INET &&
		    INTF_ADJUST_MTU_FOR_CLAT46(rt->rt_ifp)) {
			interface_mtu = IN6_LINKMTU(rt->rt_ifp);
			/* Further adjust the size for CLAT46 expansion */
			interface_mtu -= CLAT46_HDR_EXPANSION_OVERHD;
		}
		mss = interface_mtu - hlen - sizeof(struct tcphdr);
		mss = max(tcp_mssdflt, mss);
		rtfree(rt);
	}
	mss = min(mss, offer);
	mss = max(mss, 64);             /* sanity - at least max opt space */
	return mss;
}

static void
pf_set_rt_ifp(struct pf_state *s, struct pf_addr *saddr, sa_family_t af)
{
	struct pf_rule *r = s->rule.ptr;

	s->rt_kif = NULL;

	if (!r->rt || r->rt == PF_FASTROUTE) {
		return;
	}
	if ((af == AF_INET) || (af == AF_INET6)) {
		pf_map_addr(af, r, saddr, &s->rt_addr, NULL,
		    &s->nat_src_node);
		s->rt_kif = r->rpool.cur->kif;
	}

	return;
}

static void
pf_attach_state(struct pf_state_key *sk, struct pf_state *s, int tail)
{
	s->state_key = sk;
	sk->refcnt++;

	/* list is sorted, if-bound states before floating */
	if (tail) {
		TAILQ_INSERT_TAIL(&sk->states, s, next);
	} else {
		TAILQ_INSERT_HEAD(&sk->states, s, next);
	}
}

static void
pf_state_key_release_flowid(struct pf_state_key *sk)
{
#pragma unused (sk)
#if SKYWALK
	if ((sk->flowsrc == FLOWSRC_PF) && (sk->flowhash != 0)) {
		flowidns_release_flowid(sk->flowhash);
		sk->flowhash = 0;
		sk->flowsrc = 0;
	}
#endif /* SKYWALK */
}

void
pf_detach_state(struct pf_state *s, int flags)
{
	struct pf_state_key     *sk = s->state_key;

	if (sk == NULL) {
		return;
	}

	s->state_key = NULL;
	TAILQ_REMOVE(&sk->states, s, next);
	if (--sk->refcnt == 0) {
		if (!(flags & PF_DT_SKIP_EXTGWY)) {
			pf_remove_state_key_ext_gwy(sk);
		}
		if (!(flags & PF_DT_SKIP_LANEXT)) {
			RB_REMOVE(pf_state_tree_lan_ext,
			    &pf_statetbl_lan_ext, sk);
		}
		if (sk->app_state) {
			pool_put(&pf_app_state_pl, sk->app_state);
		}
		pf_state_key_release_flowid(sk);
		pool_put(&pf_state_key_pl, sk);
	}
}

struct pf_state_key *
pf_alloc_state_key(struct pf_state *s, struct pf_state_key *psk)
{
	struct pf_state_key     *__single sk;

	if ((sk = pool_get(&pf_state_key_pl, PR_WAITOK)) == NULL) {
		return NULL;
	}
	bzero(sk, sizeof(*sk));
	TAILQ_INIT(&sk->states);
	pf_attach_state(sk, s, 0);

	/* initialize state key from psk, if provided */
	if (psk != NULL) {
		bcopy(&psk->lan, &sk->lan, sizeof(sk->lan));
		bcopy(&psk->gwy, &sk->gwy, sizeof(sk->gwy));
		bcopy(&psk->ext_lan, &sk->ext_lan, sizeof(sk->ext_lan));
		bcopy(&psk->ext_gwy, &sk->ext_gwy, sizeof(sk->ext_gwy));
		sk->af_lan = psk->af_lan;
		sk->af_gwy = psk->af_gwy;
		sk->proto = psk->proto;
		sk->direction = psk->direction;
		sk->proto_variant = psk->proto_variant;
		VERIFY(psk->app_state == NULL);
		ASSERT(psk->flowsrc != FLOWSRC_PF);
		sk->flowsrc = psk->flowsrc;
		sk->flowhash = psk->flowhash;
		/* don't touch tree entries, states and refcnt on sk */
	}

	if (sk->flowhash == 0) {
		ASSERT(sk->flowsrc == 0);
		sk->flowsrc = FLOWSRC_PF;
		sk->flowhash = pf_calc_state_key_flowhash(sk);
	}

	return sk;
}

static __attribute__((noinline)) u_int32_t
pf_tcp_iss(struct pf_pdesc *pd)
{
	MD5_CTX ctx;
	u_int32_t digest[4];

	if (pf_tcp_secret_init == 0) {
		read_frandom(pf_tcp_secret, sizeof(pf_tcp_secret));
		MD5Init(&pf_tcp_secret_ctx);
		MD5Update(&pf_tcp_secret_ctx, pf_tcp_secret,
		    sizeof(pf_tcp_secret));
		pf_tcp_secret_init = 1;
	}
	ctx = pf_tcp_secret_ctx;

	MD5Update(&ctx, (char *)&pf_pd_get_hdr_tcp(pd)->th_sport, sizeof(u_short));
	MD5Update(&ctx, (char *)&pf_pd_get_hdr_tcp(pd)->th_dport, sizeof(u_short));
	if (pd->af == AF_INET6) {
		MD5Update(&ctx, (char *)&pd->src->v6addr, sizeof(struct in6_addr));
		MD5Update(&ctx, (char *)&pd->dst->v6addr, sizeof(struct in6_addr));
	} else {
		MD5Update(&ctx, (char *)&pd->src->v4addr, sizeof(struct in_addr));
		MD5Update(&ctx, (char *)&pd->dst->v4addr, sizeof(struct in_addr));
	}
	MD5Final((u_char *)digest, &ctx);
	pf_tcp_iss_off += 4096;
	return digest[0] + random() + pf_tcp_iss_off;
}

/*
 * This routine is called to perform address family translation on the
 * inner IP header (that may come as payload) of an ICMP(v4addr/6) error
 * response.
 */
static __attribute__((noinline)) int
pf_change_icmp_af(pbuf_t *pbuf, int off,
    struct pf_pdesc *pd, struct pf_pdesc *pd2, struct pf_addr *src,
    struct pf_addr *dst, sa_family_t af, sa_family_t naf)
{
	struct ip               *__single ip4 = NULL;
	struct ip6_hdr          *__single ip6 = NULL;
	void                    *__single hdr;
	int                      hlen, olen;
	uint64_t                ipid_salt = (uint64_t)pbuf_get_packet_buffer_address(pbuf);

	if (af == naf || (af != AF_INET && af != AF_INET6) ||
	    (naf != AF_INET && naf != AF_INET6)) {
		return -1;
	}

	/* old header */
	olen = pd2->off - off;
	/* new header */
	hlen = naf == AF_INET ? sizeof(*ip4) : sizeof(*ip6);

	/* Modify the pbuf to accommodate the new header */
	hdr = pbuf_resize_segment(pbuf, off, olen, hlen);
	if (hdr == NULL) {
		return -1;
	}

	/* translate inner ip/ip6 header */
	switch (naf) {
	case AF_INET:
		ip4 = hdr;
		bzero(ip4, sizeof(*ip4));
		ip4->ip_v   = IPVERSION;
		ip4->ip_hl  = sizeof(*ip4) >> 2;
		ip4->ip_len = htons(sizeof(*ip4) + pd2->tot_len - olen);
		ip4->ip_id  = rfc6864 ? 0 : htons(ip_randomid(ipid_salt));
		ip4->ip_off = htons(IP_DF);
		ip4->ip_ttl = pd2->ttl;
		if (pd2->proto == IPPROTO_ICMPV6) {
			ip4->ip_p = IPPROTO_ICMP;
		} else {
			ip4->ip_p = pd2->proto;
		}
		ip4->ip_src = src->v4addr;
		ip4->ip_dst = dst->v4addr;
		ip4->ip_sum = pbuf_inet_cksum(pbuf, 0, 0, ip4->ip_hl << 2);
		break;
	case AF_INET6:
		ip6 = hdr;
		bzero(ip6, sizeof(*ip6));
		ip6->ip6_vfc  = IPV6_VERSION;
		ip6->ip6_plen = htons(pd2->tot_len - olen);
		if (pd2->proto == IPPROTO_ICMP) {
			ip6->ip6_nxt = IPPROTO_ICMPV6;
		} else {
			ip6->ip6_nxt = pd2->proto;
		}
		if (!pd2->ttl || pd2->ttl > IPV6_DEFHLIM) {
			ip6->ip6_hlim = IPV6_DEFHLIM;
		} else {
			ip6->ip6_hlim = pd2->ttl;
		}
		ip6->ip6_src  = src->v6addr;
		ip6->ip6_dst  = dst->v6addr;
		break;
	}

	/* adjust payload offset and total packet length */
	pd2->off += hlen - olen;
	pd->tot_len += hlen - olen;

	return 0;
}

#define PTR_IP(field)   ((int32_t)offsetof(struct ip, field))
#define PTR_IP6(field)  ((int32_t)offsetof(struct ip6_hdr, field))

static __attribute__((noinline)) int
pf_translate_icmp_af(int af, void *arg)
{
	struct icmp             *__single icmp4;
	struct icmp6_hdr        *__single icmp6;
	u_int32_t                mtu;
	int32_t                  ptr = -1;
	u_int8_t                 type;
	u_int8_t                 code;

	switch (af) {
	case AF_INET:
		icmp6 = (struct icmp6_hdr * __single)arg;
		type  = icmp6->icmp6_type;
		code  = icmp6->icmp6_code;
		mtu   = ntohl(icmp6->icmp6_mtu);

		switch (type) {
		case ICMP6_ECHO_REQUEST:
			type = ICMP_ECHO;
			break;
		case ICMP6_ECHO_REPLY:
			type = ICMP_ECHOREPLY;
			break;
		case ICMP6_DST_UNREACH:
			type = ICMP_UNREACH;
			switch (code) {
			case ICMP6_DST_UNREACH_NOROUTE:
			case ICMP6_DST_UNREACH_BEYONDSCOPE:
			case ICMP6_DST_UNREACH_ADDR:
				code = ICMP_UNREACH_HOST;
				break;
			case ICMP6_DST_UNREACH_ADMIN:
				code = ICMP_UNREACH_HOST_PROHIB;
				break;
			case ICMP6_DST_UNREACH_NOPORT:
				code = ICMP_UNREACH_PORT;
				break;
			default:
				return -1;
			}
			break;
		case ICMP6_PACKET_TOO_BIG:
			type = ICMP_UNREACH;
			code = ICMP_UNREACH_NEEDFRAG;
			mtu -= 20;
			break;
		case ICMP6_TIME_EXCEEDED:
			type = ICMP_TIMXCEED;
			break;
		case ICMP6_PARAM_PROB:
			switch (code) {
			case ICMP6_PARAMPROB_HEADER:
				type = ICMP_PARAMPROB;
				code = ICMP_PARAMPROB_ERRATPTR;
				ptr  = ntohl(icmp6->icmp6_pptr);

				if (ptr == PTR_IP6(ip6_vfc)) {
					; /* preserve */
				} else if (ptr == PTR_IP6(ip6_vfc) + 1) {
					ptr = PTR_IP(ip_tos);
				} else if (ptr == PTR_IP6(ip6_plen) ||
				    ptr == PTR_IP6(ip6_plen) + 1) {
					ptr = PTR_IP(ip_len);
				} else if (ptr == PTR_IP6(ip6_nxt)) {
					ptr = PTR_IP(ip_p);
				} else if (ptr == PTR_IP6(ip6_hlim)) {
					ptr = PTR_IP(ip_ttl);
				} else if (ptr >= PTR_IP6(ip6_src) &&
				    ptr < PTR_IP6(ip6_dst)) {
					ptr = PTR_IP(ip_src);
				} else if (ptr >= PTR_IP6(ip6_dst) &&
				    ptr < (int32_t)sizeof(struct ip6_hdr)) {
					ptr = PTR_IP(ip_dst);
				} else {
					return -1;
				}
				break;
			case ICMP6_PARAMPROB_NEXTHEADER:
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_PROTOCOL;
				break;
			default:
				return -1;
			}
			break;
		default:
			return -1;
		}
		icmp6->icmp6_type = type;
		icmp6->icmp6_code = code;
		/* aligns well with a icmpv4 nextmtu */
		icmp6->icmp6_mtu = htonl(mtu);
		/* icmpv4 pptr is a one most significant byte */
		if (ptr >= 0) {
			icmp6->icmp6_pptr = htonl(ptr << 24);
		}
		break;

	case AF_INET6:
		icmp4 = (struct icmp* __single)arg;
		type  = icmp4->icmp_type;
		code  = icmp4->icmp_code;
		mtu   = ntohs(icmp4->icmp_nextmtu);

		switch (type) {
		case ICMP_ECHO:
			type = ICMP6_ECHO_REQUEST;
			break;
		case ICMP_ECHOREPLY:
			type = ICMP6_ECHO_REPLY;
			break;
		case ICMP_UNREACH:
			type = ICMP6_DST_UNREACH;
			switch (code) {
			case ICMP_UNREACH_NET:
			case ICMP_UNREACH_HOST:
			case ICMP_UNREACH_NET_UNKNOWN:
			case ICMP_UNREACH_HOST_UNKNOWN:
			case ICMP_UNREACH_ISOLATED:
			case ICMP_UNREACH_TOSNET:
			case ICMP_UNREACH_TOSHOST:
				code = ICMP6_DST_UNREACH_NOROUTE;
				break;
			case ICMP_UNREACH_PORT:
				code = ICMP6_DST_UNREACH_NOPORT;
				break;
			case ICMP_UNREACH_NET_PROHIB:
			case ICMP_UNREACH_HOST_PROHIB:
			case ICMP_UNREACH_FILTER_PROHIB:
			case ICMP_UNREACH_PRECEDENCE_CUTOFF:
				code = ICMP6_DST_UNREACH_ADMIN;
				break;
			case ICMP_UNREACH_PROTOCOL:
				type = ICMP6_PARAM_PROB;
				code = ICMP6_PARAMPROB_NEXTHEADER;
				ptr  = offsetof(struct ip6_hdr, ip6_nxt);
				break;
			case ICMP_UNREACH_NEEDFRAG:
				type = ICMP6_PACKET_TOO_BIG;
				code = 0;
				mtu += 20;
				break;
			default:
				return -1;
			}
			break;
		case ICMP_TIMXCEED:
			type = ICMP6_TIME_EXCEEDED;
			break;
		case ICMP_PARAMPROB:
			type = ICMP6_PARAM_PROB;
			switch (code) {
			case ICMP_PARAMPROB_ERRATPTR:
				code = ICMP6_PARAMPROB_HEADER;
				break;
			case ICMP_PARAMPROB_LENGTH:
				code = ICMP6_PARAMPROB_HEADER;
				break;
			default:
				return -1;
			}

			ptr = icmp4->icmp_pptr;
			if (ptr == 0 || ptr == PTR_IP(ip_tos)) {
				; /* preserve */
			} else if (ptr == PTR_IP(ip_len) ||
			    ptr == PTR_IP(ip_len) + 1) {
				ptr = PTR_IP6(ip6_plen);
			} else if (ptr == PTR_IP(ip_ttl)) {
				ptr = PTR_IP6(ip6_hlim);
			} else if (ptr == PTR_IP(ip_p)) {
				ptr = PTR_IP6(ip6_nxt);
			} else if (ptr >= PTR_IP(ip_src) &&
			    ptr < PTR_IP(ip_dst)) {
				ptr = PTR_IP6(ip6_src);
			} else if (ptr >= PTR_IP(ip_dst) &&
			    ptr < (int32_t)sizeof(struct ip)) {
				ptr = PTR_IP6(ip6_dst);
			} else {
				return -1;
			}
			break;
		default:
			return -1;
		}
		icmp4->icmp_type = type;
		icmp4->icmp_code = code;
		icmp4->icmp_nextmtu = htons(mtu);
		if (ptr >= 0) {
			icmp4->icmp_void = htonl(ptr);
		}
		break;
	}

	return 0;
}

/* Note: frees pbuf if PF_NAT64 is returned */
static __attribute__((noinline)) int
pf_nat64_ipv6(pbuf_t *pbuf, int off, struct pf_pdesc *pd)
{
	struct ip               *ip4;
	struct mbuf *m;

	/*
	 * ip_input asserts for rcvif to be not NULL
	 * That may not be true for two corner cases
	 * 1. If for some reason a local app sends DNS
	 * AAAA query to local host
	 * 2. If IPv6 stack in kernel internally generates a
	 * message destined for a synthesized IPv6 end-point.
	 */
	if (pbuf->pb_ifp == NULL) {
		return PF_DROP;
	}

	ip4 = (struct ip *)pbuf_resize_segment(pbuf, 0, off, sizeof(*ip4));
	if (ip4 == NULL) {
		return PF_DROP;
	}

	ip4->ip_v   = 4;
	ip4->ip_hl  = 5;
	ip4->ip_tos = pd->tos & htonl(0x0ff00000);
	ip4->ip_len = htons(sizeof(*ip4) + (pd->tot_len - off));
	ip4->ip_id  = 0;
	ip4->ip_off = htons(IP_DF);
	ip4->ip_ttl = pd->ttl;
	ip4->ip_p   = pd->proto;
	ip4->ip_sum = 0;
	ip4->ip_src = pd->naddr.v4addr;
	ip4->ip_dst = pd->ndaddr.v4addr;
	ip4->ip_sum = pbuf_inet_cksum(pbuf, 0, 0, ip4->ip_hl << 2);

	/* recalculate icmp checksums */
	if (pd->proto == IPPROTO_ICMP) {
		struct icmp *icmp;
		int hlen = sizeof(*ip4);

		icmp = (struct icmp *)pbuf_contig_segment(pbuf, hlen,
		    ICMP_MINLEN);
		if (icmp == NULL) {
			return PF_DROP;
		}

		icmp->icmp_cksum = 0;
		icmp->icmp_cksum = pbuf_inet_cksum(pbuf, 0, hlen,
		    ntohs(ip4->ip_len) - hlen);
	}

	if ((m = pbuf_to_mbuf(pbuf, TRUE)) != NULL) {
		ip_input(m);
	}

	return PF_NAT64;
}

static __attribute__((noinline)) int
pf_nat64_ipv4(pbuf_t *pbuf, int off, struct pf_pdesc *pd)
{
	struct ip6_hdr          *ip6;
	struct mbuf *m;

	if (pbuf->pb_ifp == NULL) {
		return PF_DROP;
	}

	ip6 = (struct ip6_hdr *)pbuf_resize_segment(pbuf, 0, off, sizeof(*ip6));
	if (ip6 == NULL) {
		return PF_DROP;
	}

	ip6->ip6_vfc  = htonl((6 << 28) | (pd->tos << 20));
	ip6->ip6_plen = htons(pd->tot_len - off);
	ip6->ip6_nxt  = pd->proto;
	ip6->ip6_hlim = pd->ttl;
	ip6->ip6_src = pd->naddr.v6addr;
	ip6->ip6_dst = pd->ndaddr.v6addr;

	/* recalculate icmp6 checksums */
	if (pd->proto == IPPROTO_ICMPV6) {
		struct icmp6_hdr *icmp6;
		int hlen = sizeof(*ip6);

		icmp6 = (struct icmp6_hdr *)pbuf_contig_segment(pbuf, hlen,
		    sizeof(*icmp6));
		if (icmp6 == NULL) {
			return PF_DROP;
		}

		icmp6->icmp6_cksum = 0;
		icmp6->icmp6_cksum = pbuf_inet6_cksum(pbuf,
		    IPPROTO_ICMPV6, hlen,
		    ntohs(ip6->ip6_plen));
	} else if (pd->proto == IPPROTO_UDP) {
		struct udphdr *uh;
		int hlen = sizeof(*ip6);

		uh = (struct udphdr *)pbuf_contig_segment(pbuf, hlen,
		    sizeof(*uh));
		if (uh == NULL) {
			return PF_DROP;
		}

		if (uh->uh_sum == 0) {
			uh->uh_sum = pbuf_inet6_cksum(pbuf, IPPROTO_UDP,
			    hlen, ntohs(ip6->ip6_plen));
		}
	}

	if ((m = pbuf_to_mbuf(pbuf, TRUE)) != NULL) {
		ip6_input(m);
	}

	return PF_NAT64;
}

static __attribute__((noinline)) int
pf_test_rule(struct pf_rule **rm, struct pf_state **sm, int direction,
    struct pfi_kif *kif, pbuf_t *pbuf, int off, void *h,
    struct pf_pdesc *pd, struct pf_rule **am, struct pf_ruleset **rsm,
    struct ifqueue *ifq)
{
#pragma unused(h)
	struct pf_rule          *__single nr = NULL;
	struct pf_addr          *__single saddr = pd->src, *__single daddr = pd->dst;
	sa_family_t              af = pd->af;
	struct pf_rule          *__single r, *__single a = NULL;
	struct pf_ruleset       *__single ruleset = NULL;
	struct pf_src_node      *__single nsn = NULL;
	struct tcphdr           *__single th = pf_pd_get_hdr_tcp(pd);
	struct udphdr           *__single uh = pf_pd_get_hdr_udp(pd);
	u_short                  reason;
	int                      rewrite = 0, hdrlen = 0;
	int                      tag = -1;
	unsigned int             rtableid = IFSCOPE_NONE;
	int                      asd = 0;
	int                      match = 0;
	int                      state_icmp = 0;
	u_int16_t                mss = tcp_mssdflt;
	u_int8_t                 icmptype = 0, icmpcode = 0;
#if SKYWALK
	struct ns_token *__single nstoken = NULL;
#endif

	struct pf_grev1_hdr     *__single grev1 = pf_pd_get_hdr_grev1(pd);
	union pf_state_xport bxport, bdxport, nxport, sxport, dxport;
	struct pf_state_key      psk;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	PD_CLEAR_STATE_FLOWID(pd);

	if (direction == PF_IN && pf_check_congestion(ifq)) {
		REASON_SET(&reason, PFRES_CONGEST);
		return PF_DROP;
	}

	hdrlen = 0;
	sxport.spi = 0;
	dxport.spi = 0;
	nxport.spi = 0;

	switch (pd->proto) {
	case IPPROTO_TCP:
		sxport.port = th->th_sport;
		dxport.port = th->th_dport;
		hdrlen = sizeof(*th);
		break;
	case IPPROTO_UDP:
		sxport.port = uh->uh_sport;
		dxport.port = uh->uh_dport;
		hdrlen = sizeof(*uh);
		break;
#if INET
	case IPPROTO_ICMP:
		if (pd->af != AF_INET) {
			break;
		}
		sxport.port = dxport.port = pf_pd_get_hdr_icmp(pd)->icmp_id;
		hdrlen = ICMP_MINLEN;
		icmptype = pf_pd_get_hdr_icmp(pd)->icmp_type;
		icmpcode = pf_pd_get_hdr_icmp(pd)->icmp_code;

		if (ICMP_ERRORTYPE(icmptype)) {
			state_icmp++;
		}
		break;
#endif /* INET */
	case IPPROTO_ICMPV6:
		if (pd->af != AF_INET6) {
			break;
		}
		sxport.port = dxport.port = pf_pd_get_hdr_icmp6(pd)->icmp6_id;
		hdrlen = sizeof(*pf_pd_get_hdr_icmp6(pd));
		icmptype = pf_pd_get_hdr_icmp6(pd)->icmp6_type;
		icmpcode = pf_pd_get_hdr_icmp6(pd)->icmp6_code;

		if (ICMP6_ERRORTYPE(icmptype)) {
			state_icmp++;
		}
		break;
	case IPPROTO_GRE:
		if (pd->proto_variant == PF_GRE_PPTP_VARIANT) {
			sxport.call_id = dxport.call_id =
			    pf_pd_get_hdr_grev1(pd)->call_id;
			hdrlen = sizeof(*pf_pd_get_hdr_grev1(pd));
		}
		break;
	case IPPROTO_ESP:
		sxport.spi = 0;
		dxport.spi = pf_pd_get_hdr_esp(pd)->spi;
		hdrlen = sizeof(*pf_pd_get_hdr_esp(pd));
		break;
	}

	r = TAILQ_FIRST(pf_main_ruleset.rules[PF_RULESET_FILTER].active.ptr);

	bxport = sxport;
	bdxport = dxport;

	if (direction == PF_OUT) {
		nxport = sxport;
	} else {
		nxport = dxport;
	}

	/* check packet for BINAT/NAT/RDR */
	if ((nr = pf_get_translation_aux(pd, pbuf, off, direction, kif, &nsn,
	    saddr, &sxport, daddr, &dxport, &nxport
#if SKYWALK
	    , &nstoken
#endif
	    )) != NULL) {
		int ua;
		u_int16_t dport;

		if (pd->af != pd->naf) {
			ua = 0;
		} else {
			ua = 1;
		}

		PF_ACPY(&pd->baddr, saddr, af);
		PF_ACPY(&pd->bdaddr, daddr, af);

		switch (pd->proto) {
		case IPPROTO_TCP:
			if (pd->af != pd->naf ||
			    PF_ANEQ(saddr, &pd->naddr, pd->af)) {
				pf_change_ap(direction, pd->mp, saddr,
				    &th->th_sport, pd->ip_sum, &th->th_sum,
				    &pd->naddr, nxport.port, 0, af,
				    pd->naf, ua);
				sxport.port = th->th_sport;
			}

			if (pd->af != pd->naf ||
			    PF_ANEQ(daddr, &pd->ndaddr, pd->af) ||
			    (nr && (nr->action == PF_RDR) &&
			    (th->th_dport != nxport.port))) {
				if (nr && nr->action == PF_RDR) {
					dport = nxport.port;
				} else {
					dport = th->th_dport;
				}
				pf_change_ap(direction, pd->mp, daddr,
				    &th->th_dport, pd->ip_sum,
				    &th->th_sum, &pd->ndaddr,
				    dport, 0, af, pd->naf, ua);
				dxport.port = th->th_dport;
			}
			rewrite++;
			break;

		case IPPROTO_UDP:
			if (pd->af != pd->naf ||
			    PF_ANEQ(saddr, &pd->naddr, pd->af)) {
				pf_change_ap(direction, pd->mp, saddr,
				    &uh->uh_sport, pd->ip_sum,
				    &uh->uh_sum, &pd->naddr,
				    nxport.port, 1, af, pd->naf, ua);
				sxport.port = uh->uh_sport;
			}

			if (pd->af != pd->naf ||
			    PF_ANEQ(daddr, &pd->ndaddr, pd->af) ||
			    (nr && (nr->action == PF_RDR) &&
			    (uh->uh_dport != nxport.port))) {
				if (nr && nr->action == PF_RDR) {
					dport = nxport.port;
				} else {
					dport = uh->uh_dport;
				}
				pf_change_ap(direction, pd->mp, daddr,
				    &uh->uh_dport, pd->ip_sum,
				    &uh->uh_sum, &pd->ndaddr,
				    dport, 0, af, pd->naf, ua);
				dxport.port = uh->uh_dport;
			}
			rewrite++;
			break;
#if INET
		case IPPROTO_ICMP:
			if (pd->af != AF_INET) {
				break;
			}
			/*
			 * TODO:
			 * pd->af != pd->naf not handled yet here and would be
			 * needed for NAT46 needed to support XLAT.
			 * Will cross the bridge when it comes.
			 */
			if (PF_ANEQ(saddr, &pd->naddr, pd->af)) {
				pf_change_a(&saddr->v4addr.s_addr, pd->ip_sum,
				    pd->naddr.v4addr.s_addr, 0);
				pf_pd_get_hdr_icmp(pd)->icmp_cksum = pf_cksum_fixup(
					pf_pd_get_hdr_icmp(pd)->icmp_cksum, sxport.port,
					nxport.port, 0);
				pf_pd_get_hdr_icmp(pd)->icmp_id = nxport.port;
			}

			if (PF_ANEQ(daddr, &pd->ndaddr, pd->af)) {
				pf_change_a(&daddr->v4addr.s_addr, pd->ip_sum,
				    pd->ndaddr.v4addr.s_addr, 0);
			}
			++rewrite;
			break;
#endif /* INET */
		case IPPROTO_ICMPV6:
			if (pd->af != AF_INET6) {
				break;
			}

			if (pd->af != pd->naf ||
			    PF_ANEQ(saddr, &pd->naddr, pd->af)) {
				pf_change_addr(saddr,
				    &pf_pd_get_hdr_icmp6(pd)->icmp6_cksum,
				    &pd->naddr, 0, pd->af, pd->naf);
			}

			if (pd->af != pd->naf ||
			    PF_ANEQ(daddr, &pd->ndaddr, pd->af)) {
				pf_change_addr(daddr,
				    &pf_pd_get_hdr_icmp6(pd)->icmp6_cksum,
				    &pd->ndaddr, 0, pd->af, pd->naf);
			}

			if (pd->af != pd->naf) {
				if (pf_translate_icmp_af(AF_INET,
				    pf_pd_get_hdr_icmp6(pd))) {
					return PF_DROP;
				}
				pd->proto = IPPROTO_ICMP;
			}
			rewrite++;
			break;
		case IPPROTO_GRE:
			if ((direction == PF_IN) &&
			    (pd->proto_variant == PF_GRE_PPTP_VARIANT)) {
				grev1->call_id = nxport.call_id;
			}

			switch (pd->af) {
#if INET
			case AF_INET:
				if (PF_ANEQ(saddr, &pd->naddr, pd->af)) {
					pf_change_a(&saddr->v4addr.s_addr,
					    pd->ip_sum,
					    pd->naddr.v4addr.s_addr, 0);
				}
				if (PF_ANEQ(daddr, &pd->ndaddr, pd->af)) {
					pf_change_a(&daddr->v4addr.s_addr,
					    pd->ip_sum,
					    pd->ndaddr.v4addr.s_addr, 0);
				}
				break;
#endif /* INET */
			case AF_INET6:
				if (PF_ANEQ(saddr, &pd->naddr, pd->af)) {
					PF_ACPY(saddr, &pd->naddr, AF_INET6);
				}
				if (PF_ANEQ(daddr, &pd->ndaddr, pd->af)) {
					PF_ACPY(daddr, &pd->ndaddr, AF_INET6);
				}
				break;
			}
			++rewrite;
			break;
		case IPPROTO_ESP:
			if (direction == PF_OUT) {
				bxport.spi = 0;
			}

			switch (pd->af) {
#if INET
			case AF_INET:
				if (PF_ANEQ(saddr, &pd->naddr, pd->af)) {
					pf_change_a(&saddr->v4addr.s_addr,
					    pd->ip_sum, pd->naddr.v4addr.s_addr, 0);
				}
				if (PF_ANEQ(daddr, &pd->ndaddr, pd->af)) {
					pf_change_a(&daddr->v4addr.s_addr,
					    pd->ip_sum,
					    pd->ndaddr.v4addr.s_addr, 0);
				}
				break;
#endif /* INET */
			case AF_INET6:
				if (PF_ANEQ(saddr, &pd->naddr, pd->af)) {
					PF_ACPY(saddr, &pd->naddr, AF_INET6);
				}
				if (PF_ANEQ(daddr, &pd->ndaddr, pd->af)) {
					PF_ACPY(daddr, &pd->ndaddr, AF_INET6);
				}
				break;
			}
			break;
		default:
			switch (pd->af) {
#if INET
			case AF_INET:
				if ((pd->naf != AF_INET) ||
				    (PF_ANEQ(saddr, &pd->naddr, pd->af))) {
					pf_change_addr(saddr, pd->ip_sum,
					    &pd->naddr, 0, af, pd->naf);
				}

				if ((pd->naf != AF_INET) ||
				    (PF_ANEQ(daddr, &pd->ndaddr, pd->af))) {
					pf_change_addr(daddr, pd->ip_sum,
					    &pd->ndaddr, 0, af, pd->naf);
				}
				break;
#endif /* INET */
			case AF_INET6:
				if (PF_ANEQ(saddr, &pd->naddr, pd->af)) {
					PF_ACPY(saddr, &pd->naddr, af);
				}
				if (PF_ANEQ(daddr, &pd->ndaddr, pd->af)) {
					PF_ACPY(daddr, &pd->ndaddr, af);
				}
				break;
			}
			break;
		}

		if (nr->natpass) {
			r = NULL;
		}
		pd->nat_rule = nr;
		pd->af = pd->naf;
	} else {
#if SKYWALK
		VERIFY(!NETNS_TOKEN_VALID(&nstoken));
#endif
	}

	if (nr && nr->tag > 0) {
		tag = nr->tag;
	}

	while (r != NULL) {
		r->evaluations++;
		if (pfi_kif_match(r->kif, kif) == r->ifnot) {
			r = r->skip[PF_SKIP_IFP].ptr;
		} else if (r->direction && r->direction != direction) {
			r = r->skip[PF_SKIP_DIR].ptr;
		} else if (r->af && r->af != pd->af) {
			r = r->skip[PF_SKIP_AF].ptr;
		} else if (r->proto && r->proto != pd->proto) {
			r = r->skip[PF_SKIP_PROTO].ptr;
		} else if (PF_MISMATCHAW(&r->src.addr, saddr, pd->af,
		    r->src.neg, kif)) {
			r = r->skip[PF_SKIP_SRC_ADDR].ptr;
		}
		/* tcp/udp only. port_op always 0 in other cases */
		else if (r->proto == pd->proto &&
		    (r->proto == IPPROTO_TCP || r->proto == IPPROTO_UDP) &&
		    r->src.xport.range.op &&
		    !pf_match_port(r->src.xport.range.op,
		    r->src.xport.range.port[0], r->src.xport.range.port[1],
		    th->th_sport)) {
			r = r->skip[PF_SKIP_SRC_PORT].ptr;
		} else if (PF_MISMATCHAW(&r->dst.addr, daddr, pd->af,
		    r->dst.neg, NULL)) {
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		}
		/* tcp/udp only. port_op always 0 in other cases */
		else if (r->proto == pd->proto &&
		    (r->proto == IPPROTO_TCP || r->proto == IPPROTO_UDP) &&
		    r->dst.xport.range.op &&
		    !pf_match_port(r->dst.xport.range.op,
		    r->dst.xport.range.port[0], r->dst.xport.range.port[1],
		    th->th_dport)) {
			r = r->skip[PF_SKIP_DST_PORT].ptr;
		}
		/* icmp only. type always 0 in other cases */
		else if (r->type && r->type != icmptype + 1) {
			r = TAILQ_NEXT(r, entries);
		}
		/* icmp only. type always 0 in other cases */
		else if (r->code && r->code != icmpcode + 1) {
			r = TAILQ_NEXT(r, entries);
		} else if ((r->rule_flag & PFRULE_TOS) && r->tos &&
		    !(r->tos & pd->tos)) {
			r = TAILQ_NEXT(r, entries);
		} else if ((r->rule_flag & PFRULE_DSCP) && r->tos &&
		    !(r->tos & (pd->tos & DSCP_MASK))) {
			r = TAILQ_NEXT(r, entries);
		} else if ((r->rule_flag & PFRULE_SC) && r->tos &&
		    ((r->tos & SCIDX_MASK) != pd->sc)) {
			r = TAILQ_NEXT(r, entries);
		} else if (r->rule_flag & PFRULE_FRAGMENT) {
			r = TAILQ_NEXT(r, entries);
		} else if (pd->proto == IPPROTO_TCP &&
		    (r->flagset & th->th_flags) != r->flags) {
			r = TAILQ_NEXT(r, entries);
		}
		/* tcp/udp only. uid.op always 0 in other cases */
		else if (r->uid.op && (pd->lookup.done || ((void)(pd->lookup.done =
		    pf_socket_lookup(direction, pd)), 1)) &&
		    !pf_match_uid(r->uid.op, r->uid.uid[0], r->uid.uid[1],
		    pd->lookup.uid)) {
			r = TAILQ_NEXT(r, entries);
		}
		/* tcp/udp only. gid.op always 0 in other cases */
		else if (r->gid.op && (pd->lookup.done || ((void)(pd->lookup.done =
		    pf_socket_lookup(direction, pd)), 1)) &&
		    !pf_match_gid(r->gid.op, r->gid.gid[0], r->gid.gid[1],
		    pd->lookup.gid)) {
			r = TAILQ_NEXT(r, entries);
		} else if (r->prob && r->prob <= (RandomULong() % (UINT_MAX - 1) + 1)) {
			r = TAILQ_NEXT(r, entries);
		} else if (r->match_tag && !pf_match_tag(r, pd->pf_mtag, &tag)) {
			r = TAILQ_NEXT(r, entries);
		} else if (r->os_fingerprint != PF_OSFP_ANY &&
		    (pd->proto != IPPROTO_TCP || !pf_osfp_match(
			    pf_osfp_fingerprint(pd, pbuf, off, th),
			    r->os_fingerprint))) {
			r = TAILQ_NEXT(r, entries);
		} else {
			if (r->tag) {
				tag = r->tag;
			}
			if (PF_RTABLEID_IS_VALID(r->rtableid)) {
				rtableid = r->rtableid;
			}
			if (r->anchor == NULL) {
				match = 1;
				*rm = r;
				*am = a;
				*rsm = ruleset;
				if ((*rm)->quick) {
					break;
				}
				r = TAILQ_NEXT(r, entries);
			} else {
				pf_step_into_anchor(&asd, &ruleset,
				    PF_RULESET_FILTER, &r, &a, &match);
			}
		}
		if (r == NULL && pf_step_out_of_anchor(&asd, &ruleset,
		    PF_RULESET_FILTER, &r, &a, &match)) {
			break;
		}
	}
	r = *rm;
	a = *am;
	ruleset = *rsm;

	REASON_SET(&reason, PFRES_MATCH);

	if (r->log || (nr != NULL && nr->log)) {
		if (rewrite > 0) {
			if (rewrite < off + pd->hdrlen) {
				rewrite = off + pd->hdrlen;
			}

			if (pf_lazy_makewritable(pd, pbuf, rewrite) == NULL) {
				REASON_SET(&reason, PFRES_MEMORY);
#if SKYWALK
				netns_release(&nstoken);
#endif
				return PF_DROP;
			}
			pbuf_copy_back(pbuf, off, pd->hdrlen, pf_pd_get_hdr_ptr_any(pd), pd->hdrlen);
		}
		PFLOG_PACKET(kif, h, pbuf, pd->af, direction, reason,
		    r->log ? r : nr, a, ruleset, pd);
	}

	if ((r->action == PF_DROP) &&
	    ((r->rule_flag & PFRULE_RETURNRST) ||
	    (r->rule_flag & PFRULE_RETURNICMP) ||
	    (r->rule_flag & PFRULE_RETURN))) {
		/* undo NAT changes, if they have taken place */
		/* XXX For NAT64 we are not reverting the changes */
		if (nr != NULL && nr->action != PF_NAT64) {
			if (direction == PF_OUT) {
				pd->af = af;
				switch (pd->proto) {
				case IPPROTO_TCP:
					pf_change_ap(direction, pd->mp, saddr,
					    &th->th_sport, pd->ip_sum,
					    &th->th_sum, &pd->baddr,
					    bxport.port, 0, af, pd->af, 1);
					sxport.port = th->th_sport;
					rewrite++;
					break;
				case IPPROTO_UDP:
					pf_change_ap(direction, pd->mp, saddr,
					    &pf_pd_get_hdr_udp(pd)->uh_sport, pd->ip_sum,
					    &pf_pd_get_hdr_udp(pd)->uh_sum, &pd->baddr,
					    bxport.port, 1, af, pd->af, 1);
					sxport.port = pf_pd_get_hdr_udp(pd)->uh_sport;
					rewrite++;
					break;
				case IPPROTO_ICMP:
				case IPPROTO_ICMPV6:
					/* nothing! */
					break;
				case IPPROTO_GRE:
					PF_ACPY(&pd->baddr, saddr, af);
					++rewrite;
					switch (af) {
#if INET
					case AF_INET:
						pf_change_a(&saddr->v4addr.s_addr,
						    pd->ip_sum,
						    pd->baddr.v4addr.s_addr, 0);
						break;
#endif /* INET */
					case AF_INET6:
						PF_ACPY(saddr, &pd->baddr,
						    AF_INET6);
						break;
					}
					break;
				case IPPROTO_ESP:
					PF_ACPY(&pd->baddr, saddr, af);
					switch (af) {
#if INET
					case AF_INET:
						pf_change_a(&saddr->v4addr.s_addr,
						    pd->ip_sum,
						    pd->baddr.v4addr.s_addr, 0);
						break;
#endif /* INET */
					case AF_INET6:
						PF_ACPY(saddr, &pd->baddr,
						    AF_INET6);
						break;
					}
					break;
				default:
					switch (af) {
					case AF_INET:
						pf_change_a(&saddr->v4addr.s_addr,
						    pd->ip_sum,
						    pd->baddr.v4addr.s_addr, 0);
						break;
					case AF_INET6:
						PF_ACPY(saddr, &pd->baddr, af);
						break;
					}
				}
			} else {
				switch (pd->proto) {
				case IPPROTO_TCP:
					pf_change_ap(direction, pd->mp, daddr,
					    &th->th_dport, pd->ip_sum,
					    &th->th_sum, &pd->bdaddr,
					    bdxport.port, 0, af, pd->af, 1);
					dxport.port = th->th_dport;
					rewrite++;
					break;
				case IPPROTO_UDP:
					pf_change_ap(direction, pd->mp, daddr,
					    &pf_pd_get_hdr_udp(pd)->uh_dport, pd->ip_sum,
					    &pf_pd_get_hdr_udp(pd)->uh_sum, &pd->bdaddr,
					    bdxport.port, 1, af, pd->af, 1);
					dxport.port = pf_pd_get_hdr_udp(pd)->uh_dport;
					rewrite++;
					break;
				case IPPROTO_ICMP:
				case IPPROTO_ICMPV6:
					/* nothing! */
					break;
				case IPPROTO_GRE:
					if (pd->proto_variant ==
					    PF_GRE_PPTP_VARIANT) {
						grev1->call_id =
						    bdxport.call_id;
					}
					++rewrite;
					switch (af) {
#if INET
					case AF_INET:
						pf_change_a(&daddr->v4addr.s_addr,
						    pd->ip_sum,
						    pd->bdaddr.v4addr.s_addr, 0);
						break;
#endif /* INET */
					case AF_INET6:
						PF_ACPY(daddr, &pd->bdaddr,
						    AF_INET6);
						break;
					}
					break;
				case IPPROTO_ESP:
					switch (af) {
#if INET
					case AF_INET:
						pf_change_a(&daddr->v4addr.s_addr,
						    pd->ip_sum,
						    pd->bdaddr.v4addr.s_addr, 0);
						break;
#endif /* INET */
					case AF_INET6:
						PF_ACPY(daddr, &pd->bdaddr,
						    AF_INET6);
						break;
					}
					break;
				default:
					switch (af) {
					case AF_INET:
						pf_change_a(&daddr->v4addr.s_addr,
						    pd->ip_sum,
						    pd->bdaddr.v4addr.s_addr, 0);
						break;
					case AF_INET6:
						PF_ACPY(daddr, &pd->bdaddr, af);
						break;
					}
				}
			}
		}
		if (pd->proto == IPPROTO_TCP &&
		    ((r->rule_flag & PFRULE_RETURNRST) ||
		    (r->rule_flag & PFRULE_RETURN)) &&
		    !(th->th_flags & TH_RST)) {
			u_int32_t        ack = ntohl(th->th_seq) + pd->p_len;
			int              len = 0;
			struct ip       *__single h4;
			struct ip6_hdr  *__single h6;

			switch (pd->af) {
			case AF_INET:
				h4 = pbuf->pb_data;
				len = ntohs(h4->ip_len) - off;
				break;
			case AF_INET6:
				h6 = pbuf->pb_data;
				len = ntohs(h6->ip6_plen) -
				    (off - sizeof(*h6));
				break;
			}

			if (pf_check_proto_cksum(pbuf, off, len, IPPROTO_TCP,
			    pd->af)) {
				REASON_SET(&reason, PFRES_PROTCKSUM);
			} else {
				if (th->th_flags & TH_SYN) {
					ack++;
				}
				if (th->th_flags & TH_FIN) {
					ack++;
				}
				pf_send_tcp(r, pd->af, pd->dst,
				    pd->src, th->th_dport, th->th_sport,
				    ntohl(th->th_ack), ack, TH_RST | TH_ACK, 0, 0,
				    r->return_ttl, 1, 0, pd->eh, kif->pfik_ifp);
			}
		} else if (pd->proto != IPPROTO_ICMP && pd->af == AF_INET &&
		    pd->proto != IPPROTO_ESP && pd->proto != IPPROTO_AH &&
		    r->return_icmp) {
			pf_send_icmp(pbuf, r->return_icmp >> 8,
			    r->return_icmp & 255, pd->af, r);
		} else if (pd->proto != IPPROTO_ICMPV6 && af == AF_INET6 &&
		    pd->proto != IPPROTO_ESP && pd->proto != IPPROTO_AH &&
		    r->return_icmp6) {
			pf_send_icmp(pbuf, r->return_icmp6 >> 8,
			    r->return_icmp6 & 255, pd->af, r);
		}
	}

	if (r->action == PF_DROP) {
#if SKYWALK
		netns_release(&nstoken);
#endif
		return PF_DROP;
	}

	/* prepare state key, for flowhash and/or the state (if created) */
	bzero(&psk, sizeof(psk));
	psk.proto = pd->proto;
	psk.direction = direction;
	if (pd->proto == IPPROTO_UDP) {
		if (ntohs(pf_pd_get_hdr_udp(pd)->uh_sport) == PF_IKE_PORT &&
		    ntohs(pf_pd_get_hdr_udp(pd)->uh_dport) == PF_IKE_PORT) {
			psk.proto_variant = PF_EXTFILTER_APD;
		} else {
			psk.proto_variant = nr ? nr->extfilter : r->extfilter;
			if (psk.proto_variant < PF_EXTFILTER_APD) {
				psk.proto_variant = PF_EXTFILTER_APD;
			}
		}
	} else if (pd->proto == IPPROTO_GRE) {
		psk.proto_variant = pd->proto_variant;
	}
	if (direction == PF_OUT) {
		psk.af_gwy = af;
		PF_ACPY(&psk.gwy.addr, saddr, af);
		PF_ACPY(&psk.ext_gwy.addr, daddr, af);
		switch (pd->proto) {
		case IPPROTO_ESP:
			psk.gwy.xport.spi = 0;
			psk.ext_gwy.xport.spi = pf_pd_get_hdr_esp(pd)->spi;
			break;
		case IPPROTO_ICMP:
		case IPPROTO_ICMPV6:
			/*
			 * NAT64 requires protocol translation  between ICMPv4
			 * and ICMPv6. TCP and UDP do not require protocol
			 * translation. To avoid adding complexity just to
			 * handle ICMP(v4addr/v6addr), we always lookup  for
			 * proto = IPPROTO_ICMP on both LAN and WAN side
			 */
			psk.proto = IPPROTO_ICMP;
			psk.gwy.xport.port = nxport.port;
			psk.ext_gwy.xport.spi = 0;
			break;
		default:
			psk.gwy.xport = sxport;
			psk.ext_gwy.xport = dxport;
			break;
		}
		psk.af_lan = af;
		if (nr != NULL) {
			PF_ACPY(&psk.lan.addr, &pd->baddr, af);
			psk.lan.xport = bxport;
			PF_ACPY(&psk.ext_lan.addr, &pd->bdaddr, af);
			psk.ext_lan.xport = bdxport;
		} else {
			PF_ACPY(&psk.lan.addr, &psk.gwy.addr, af);
			psk.lan.xport = psk.gwy.xport;
			PF_ACPY(&psk.ext_lan.addr, &psk.ext_gwy.addr, af);
			psk.ext_lan.xport = psk.ext_gwy.xport;
		}
	} else {
		psk.af_lan = af;
		if (nr && nr->action == PF_NAT64) {
			PF_ACPY(&psk.lan.addr, &pd->baddr, af);
			PF_ACPY(&psk.ext_lan.addr, &pd->bdaddr, af);
		} else {
			PF_ACPY(&psk.lan.addr, daddr, af);
			PF_ACPY(&psk.ext_lan.addr, saddr, af);
		}
		switch (pd->proto) {
		case IPPROTO_ICMP:
		case IPPROTO_ICMPV6:
			/*
			 * NAT64 requires protocol translation  between ICMPv4
			 * and ICMPv6. TCP and UDP do not require protocol
			 * translation. To avoid adding complexity just to
			 * handle ICMP(v4addr/v6addr), we always lookup  for
			 * proto = IPPROTO_ICMP on both LAN and WAN side
			 */
			psk.proto = IPPROTO_ICMP;
			if (nr && nr->action == PF_NAT64) {
				psk.lan.xport = bxport;
				psk.ext_lan.xport = bxport;
			} else {
				psk.lan.xport = nxport;
				psk.ext_lan.xport.spi = 0;
			}
			break;
		case IPPROTO_ESP:
			psk.ext_lan.xport.spi = 0;
			psk.lan.xport.spi = pf_pd_get_hdr_esp(pd)->spi;
			break;
		default:
			if (nr != NULL) {
				if (nr->action == PF_NAT64) {
					psk.lan.xport = bxport;
					psk.ext_lan.xport = bdxport;
				} else {
					psk.lan.xport = dxport;
					psk.ext_lan.xport = sxport;
				}
			} else {
				psk.lan.xport = dxport;
				psk.ext_lan.xport = sxport;
			}
			break;
		}
		psk.af_gwy = pd->naf;
		if (nr != NULL) {
			if (nr->action == PF_NAT64) {
				PF_ACPY(&psk.gwy.addr, &pd->naddr, pd->naf);
				PF_ACPY(&psk.ext_gwy.addr, &pd->ndaddr,
				    pd->naf);
				if ((pd->proto == IPPROTO_ICMPV6) ||
				    (pd->proto == IPPROTO_ICMP)) {
					psk.gwy.xport = nxport;
					psk.ext_gwy.xport = nxport;
				} else {
					psk.gwy.xport = sxport;
					psk.ext_gwy.xport = dxport;
				}
			} else {
				PF_ACPY(&psk.gwy.addr, &pd->bdaddr, af);
				psk.gwy.xport = bdxport;
				PF_ACPY(&psk.ext_gwy.addr, saddr, af);
				psk.ext_gwy.xport = sxport;
			}
		} else {
			PF_ACPY(&psk.gwy.addr, &psk.lan.addr, af);
			psk.gwy.xport = psk.lan.xport;
			PF_ACPY(&psk.ext_gwy.addr, &psk.ext_lan.addr, af);
			psk.ext_gwy.xport = psk.ext_lan.xport;
		}
	}
	if (pd->pktflags & PKTF_FLOW_ID) {
		/* flow hash was already computed outside of PF */
		psk.flowsrc = pd->flowsrc;
		psk.flowhash = pd->flowhash;
	} else {
		/*
		 * Allocation of flow identifier is deferred until a PF state
		 * creation is needed for this flow.
		 */
		pd->pktflags &= ~PKTF_FLOW_ADV;
		pd->flowhash = 0;
	}

	if (__improbable(pf_tag_packet(pbuf, pd->pf_mtag, tag, rtableid, pd))) {
		REASON_SET(&reason, PFRES_MEMORY);
#if SKYWALK
		netns_release(&nstoken);
#endif
		return PF_DROP;
	}

	if (!state_icmp && (r->keep_state || nr != NULL ||
	    (pd->flags & PFDESC_TCP_NORM))) {
		/* create new state */
		struct pf_state *__single s = NULL;
		struct pf_state_key *__single sk = NULL;
		struct pf_src_node *__single sn = NULL;
		struct pf_ike_hdr ike;

		if (pd->proto == IPPROTO_UDP) {
			size_t plen = pbuf->pb_packet_len - off - sizeof(*uh);

			if (ntohs(uh->uh_sport) == PF_IKE_PORT &&
			    ntohs(uh->uh_dport) == PF_IKE_PORT &&
			    plen >= PF_IKE_PACKET_MINSIZE) {
				if (plen > PF_IKE_PACKET_MINSIZE) {
					plen = PF_IKE_PACKET_MINSIZE;
				}
				pbuf_copy_data(pbuf, off + sizeof(*uh), plen,
				    &ike, sizeof(ike));
			}
		}

		if (nr != NULL && pd->proto == IPPROTO_ESP &&
		    direction == PF_OUT) {
			struct pf_state_key_cmp sk0;
			struct pf_state *s0;

			/*
			 * <jhw@apple.com>
			 * This squelches state creation if the external
			 * address matches an existing incomplete state with a
			 * different internal address.  Only one 'blocking'
			 * partial state is allowed for each external address.
			 */
#if SKYWALK
			/*
			 * XXXSCW:
			 *
			 * It's not clear how this impacts netns. The original
			 * state will hold the port reservation token but what
			 * happens to other "Cone NAT" states when the first is
			 * torn down?
			 */
#endif
			memset(&sk0, 0, sizeof(sk0));
			sk0.af_gwy = pd->af;
			sk0.proto = IPPROTO_ESP;
			PF_ACPY(&sk0.gwy.addr, saddr, sk0.af_gwy);
			PF_ACPY(&sk0.ext_gwy.addr, daddr, sk0.af_gwy);
			s0 = pf_find_state(kif, &sk0, PF_IN);

			if (s0 && PF_ANEQ(&s0->state_key->lan.addr,
			    pd->src, pd->af)) {
				nsn = 0;
				goto cleanup;
			}
		}

		/* check maximums */
		if (r->max_states && (r->states >= r->max_states)) {
			pf_status.lcounters[LCNT_STATES]++;
			REASON_SET(&reason, PFRES_MAXSTATES);
			goto cleanup;
		}
		/* src node for filter rule */
		if ((r->rule_flag & PFRULE_SRCTRACK ||
		    r->rpool.opts & PF_POOL_STICKYADDR) &&
		    pf_insert_src_node(&sn, r, saddr, af) != 0) {
			REASON_SET(&reason, PFRES_SRCLIMIT);
			goto cleanup;
		}
		/* src node for translation rule */
		if (nr != NULL && (nr->rpool.opts & PF_POOL_STICKYADDR) &&
		    ((direction == PF_OUT &&
		    nr->action != PF_RDR &&
		    pf_insert_src_node(&nsn, nr, &pd->baddr, af) != 0) ||
		    (pf_insert_src_node(&nsn, nr, saddr, af) != 0))) {
			REASON_SET(&reason, PFRES_SRCLIMIT);
			goto cleanup;
		}
		s = pool_get(&pf_state_pl, PR_WAITOK);
		if (s == NULL) {
			REASON_SET(&reason, PFRES_MEMORY);
cleanup:
			if (sn != NULL && sn->states == 0 && sn->expire == 0) {
				RB_REMOVE(pf_src_tree, &tree_src_tracking, sn);
				pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
				pf_status.src_nodes--;
				pool_put(&pf_src_tree_pl, sn);
			}
			if (nsn != sn && nsn != NULL && nsn->states == 0 &&
			    nsn->expire == 0) {
				RB_REMOVE(pf_src_tree, &tree_src_tracking, nsn);
				pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
				pf_status.src_nodes--;
				pool_put(&pf_src_tree_pl, nsn);
			}
			if (s != NULL) {
				pf_detach_state(s, 0);
			} else if (sk != NULL) {
				if (sk->app_state) {
					pool_put(&pf_app_state_pl,
					    sk->app_state);
				}
				pf_state_key_release_flowid(sk);
				pool_put(&pf_state_key_pl, sk);
			}
#if SKYWALK
			netns_release(&nstoken);
#endif
			return PF_DROP;
		}
		bzero(s, sizeof(*s));
		TAILQ_INIT(&s->unlink_hooks);
		s->rule.ptr = r;
		s->nat_rule.ptr = nr;
		s->anchor.ptr = a;
		STATE_INC_COUNTERS(s);
		s->allow_opts = r->allow_opts;
		s->log = r->log & PF_LOG_ALL;
		if (nr != NULL) {
			s->log |= nr->log & PF_LOG_ALL;
		}
		switch (pd->proto) {
		case IPPROTO_TCP:
			s->src.seqlo = ntohl(th->th_seq);
			s->src.seqhi = s->src.seqlo + pd->p_len + 1;
			if ((th->th_flags & (TH_SYN | TH_ACK)) ==
			    TH_SYN && r->keep_state == PF_STATE_MODULATE) {
				/* Generate sequence number modulator */
				if ((s->src.seqdiff = pf_tcp_iss(pd) -
				    s->src.seqlo) == 0) {
					s->src.seqdiff = 1;
				}
				pf_change_a(&th->th_seq, &th->th_sum,
				    htonl(s->src.seqlo + s->src.seqdiff), 0);
				rewrite = off + sizeof(*th);
			} else {
				s->src.seqdiff = 0;
			}
			if (th->th_flags & TH_SYN) {
				s->src.seqhi++;
				s->src.wscale = pf_get_wscale(pbuf, off,
				    th->th_off, af);
			}
			s->src.max_win = MAX(ntohs(th->th_win), 1);
			if (s->src.wscale & PF_WSCALE_MASK) {
				/* Remove scale factor from initial window */
				int win = s->src.max_win;
				win += 1 << (s->src.wscale & PF_WSCALE_MASK);
				s->src.max_win = (win - 1) >>
				    (s->src.wscale & PF_WSCALE_MASK);
			}
			if (th->th_flags & TH_FIN) {
				s->src.seqhi++;
			}
			s->dst.seqhi = 1;
			s->dst.max_win = 1;
			s->src.state = TCPS_SYN_SENT;
			s->dst.state = TCPS_CLOSED;
			s->timeout = PFTM_TCP_FIRST_PACKET;
			break;
		case IPPROTO_UDP:
			s->src.state = PFUDPS_SINGLE;
			s->dst.state = PFUDPS_NO_TRAFFIC;
			s->timeout = PFTM_UDP_FIRST_PACKET;
			break;
		case IPPROTO_ICMP:
		case IPPROTO_ICMPV6:
			s->timeout = PFTM_ICMP_FIRST_PACKET;
			break;
		case IPPROTO_GRE:
			s->src.state = PFGRE1S_INITIATING;
			s->dst.state = PFGRE1S_NO_TRAFFIC;
			s->timeout = PFTM_GREv1_INITIATING;
			break;
		case IPPROTO_ESP:
			s->src.state = PFESPS_INITIATING;
			s->dst.state = PFESPS_NO_TRAFFIC;
			s->timeout = PFTM_ESP_FIRST_PACKET;
			break;
		default:
			s->src.state = PFOTHERS_SINGLE;
			s->dst.state = PFOTHERS_NO_TRAFFIC;
			s->timeout = PFTM_OTHER_FIRST_PACKET;
		}

		s->creation = pf_time_second();
		s->expire = pf_time_second();

		if (sn != NULL) {
			s->src_node = sn;
			s->src_node->states++;
			VERIFY(s->src_node->states != 0);
		}
		if (nsn != NULL) {
			PF_ACPY(&nsn->raddr, &pd->naddr, af);
			s->nat_src_node = nsn;
			s->nat_src_node->states++;
			VERIFY(s->nat_src_node->states != 0);
		}
		if (pd->proto == IPPROTO_TCP) {
			if ((pd->flags & PFDESC_TCP_NORM) &&
			    pf_normalize_tcp_init(pbuf, off, pd, th, &s->src,
			    &s->dst)) {
				REASON_SET(&reason, PFRES_MEMORY);
				pf_src_tree_remove_state(s);
				STATE_DEC_COUNTERS(s);
#if SKYWALK
				netns_release(&nstoken);
#endif
				pool_put(&pf_state_pl, s);
				return PF_DROP;
			}
			if ((pd->flags & PFDESC_TCP_NORM) && s->src.scrub &&
			    pf_normalize_tcp_stateful(pbuf, off, pd, &reason,
			    th, s, &s->src, &s->dst, &rewrite)) {
				/* This really shouldn't happen!!! */
				DPFPRINTF(PF_DEBUG_URGENT,
				    ("pf_normalize_tcp_stateful failed on "
				    "first pkt"));
#if SKYWALK
				netns_release(&nstoken);
#endif
				pf_normalize_tcp_cleanup(s);
				pf_src_tree_remove_state(s);
				STATE_DEC_COUNTERS(s);
				pool_put(&pf_state_pl, s);
				return PF_DROP;
			}
		}

		/* allocate state key and import values from psk */
		if (__improbable((sk = pf_alloc_state_key(s, &psk)) == NULL)) {
			REASON_SET(&reason, PFRES_MEMORY);
			/*
			 * XXXSCW: This will leak the freshly-allocated
			 * state structure 's'. Although it should
			 * eventually be aged-out and removed.
			 */
			goto cleanup;
		}

		if (pd->flowhash == 0) {
			ASSERT(sk->flowhash != 0);
			ASSERT(sk->flowsrc != 0);
			pd->flowsrc = sk->flowsrc;
			pd->flowhash = sk->flowhash;
			pd->pktflags |= PKTF_FLOW_ID;
			pd->pktflags &= ~PKTF_FLOW_ADV;
			if (__improbable(pf_tag_packet(pbuf, pd->pf_mtag,
			    tag, rtableid, pd))) {
				/*
				 * this shouldn't fail as the packet tag has
				 * already been allocated.
				 */
				panic_plain("pf_tag_packet failed");
			}
		}

		pf_set_rt_ifp(s, saddr, af);    /* needs s->state_key set */

		pbuf = pd->mp; // XXXSCW: Why?

		if (sk->app_state == 0) {
			switch (pd->proto) {
			case IPPROTO_TCP: {
				u_int16_t dport = (direction == PF_OUT) ?
				    sk->ext_gwy.xport.port : sk->gwy.xport.port;

				if (nr != NULL &&
				    ntohs(dport) == PF_PPTP_PORT) {
					struct pf_app_state *__single as;

					as = pool_get(&pf_app_state_pl,
					    PR_WAITOK);
					if (!as) {
						REASON_SET(&reason,
						    PFRES_MEMORY);
						goto cleanup;
					}

					bzero(as, sizeof(*as));
					as->handler = pf_pptp_handler;
					as->compare_lan_ext = 0;
					as->compare_ext_gwy = 0;
					as->u.pptp.grev1_state = 0;
					sk->app_state = as;
					(void) hook_establish(&s->unlink_hooks,
					    0, (hook_fn_t) pf_pptp_unlink, s);
				}
				break;
			}

			case IPPROTO_UDP: {
				if (nr != NULL &&
				    ntohs(uh->uh_sport) == PF_IKE_PORT &&
				    ntohs(uh->uh_dport) == PF_IKE_PORT) {
					struct pf_app_state *__single as;

					as = pool_get(&pf_app_state_pl,
					    PR_WAITOK);
					if (!as) {
						REASON_SET(&reason,
						    PFRES_MEMORY);
						goto cleanup;
					}

					bzero(as, sizeof(*as));
					as->compare_lan_ext = pf_ike_compare;
					as->compare_ext_gwy = pf_ike_compare;
					as->u.ike.cookie = ike.initiator_cookie;
					sk->app_state = as;
				}
				break;
			}

			default:
				break;
			}
		}

		if (__improbable(pf_insert_state(BOUND_IFACE(r, kif), s))) {
			if (pd->proto == IPPROTO_TCP) {
				pf_normalize_tcp_cleanup(s);
			}
			REASON_SET(&reason, PFRES_STATEINS);
			pf_src_tree_remove_state(s);
			STATE_DEC_COUNTERS(s);
#if SKYWALK
			netns_release(&nstoken);
#endif
			pool_put(&pf_state_pl, s);
			return PF_DROP;
		} else {
#if SKYWALK
			s->nstoken = nstoken;
			nstoken = NULL;
#endif
			*sm = s;
		}
		if (tag > 0) {
			pf_tag_ref(tag);
			s->tag = tag;
		}
		if (pd->proto == IPPROTO_TCP &&
		    (th->th_flags & (TH_SYN | TH_ACK)) == TH_SYN &&
		    r->keep_state == PF_STATE_SYNPROXY) {
			int ua = (sk->af_lan == sk->af_gwy) ? 1 : 0;
			s->src.state = PF_TCPS_PROXY_SRC;
			if (nr != NULL) {
				if (direction == PF_OUT) {
					pf_change_ap(direction, pd->mp, saddr,
					    &th->th_sport, pd->ip_sum,
					    &th->th_sum, &pd->baddr,
					    bxport.port, 0, af, pd->af, ua);
					sxport.port = th->th_sport;
				} else {
					pf_change_ap(direction, pd->mp, daddr,
					    &th->th_dport, pd->ip_sum,
					    &th->th_sum, &pd->baddr,
					    bxport.port, 0, af, pd->af, ua);
					sxport.port = th->th_dport;
				}
			}
			s->src.seqhi = htonl(random());
			/* Find mss option */
			mss = pf_get_mss(pbuf, off, th->th_off, af);
			mss = pf_calc_mss(saddr, af, mss);
			mss = pf_calc_mss(daddr, af, mss);
			s->src.mss = mss;
			pf_send_tcp(r, af, daddr, saddr, th->th_dport,
			    th->th_sport, s->src.seqhi, ntohl(th->th_seq) + 1,
			    TH_SYN | TH_ACK, 0, s->src.mss, 0, 1, 0, NULL, NULL);
			REASON_SET(&reason, PFRES_SYNPROXY);
			return PF_SYNPROXY_DROP;
		}

		if (sk->app_state && sk->app_state->handler) {
			int offx = off;

			switch (pd->proto) {
			case IPPROTO_TCP:
				offx += th->th_off << 2;
				break;
			case IPPROTO_UDP:
				offx += pf_pd_get_hdr_udp(pd)->uh_ulen << 2;
				break;
			default:
				/* ALG handlers only apply to TCP and UDP rules */
				break;
			}

			if (offx > off) {
				sk->app_state->handler(s, direction, offx,
				    pd, kif);
				if (pd->lmw < 0) {
					REASON_SET(&reason, PFRES_MEMORY);
					return PF_DROP;
				}
				pbuf = pd->mp;  // XXXSCW: Why?
			}
		}
	}
#if SKYWALK
	else {
		netns_release(&nstoken);
	}
#endif

	/* copy back packet headers if we performed NAT operations */
	if (rewrite) {
		if (rewrite < off + pd->hdrlen) {
			rewrite = off + pd->hdrlen;
		}

		if (pf_lazy_makewritable(pd, pd->mp, rewrite) == NULL) {
			REASON_SET(&reason, PFRES_MEMORY);
			return PF_DROP;
		}

		pbuf_copy_back(pbuf, off, hdrlen, pf_pd_get_hdr_ptr_any(pd), pd->hdrlen);
		if (af == AF_INET6 && pd->naf == AF_INET) {
			return pf_nat64_ipv6(pbuf, off, pd);
		} else if (af == AF_INET && pd->naf == AF_INET6) {
			return pf_nat64_ipv4(pbuf, off, pd);
		}
	}

	return PF_PASS;
}

boolean_t is_nlc_enabled_glb = FALSE;

static inline boolean_t
pf_is_dummynet_enabled(void)
{
#if DUMMYNET
	if (__probable(!PF_IS_ENABLED)) {
		return FALSE;
	}

	if (__probable(!DUMMYNET_LOADED)) {
		return FALSE;
	}

	if (__probable(TAILQ_EMPTY(pf_main_ruleset.
	    rules[PF_RULESET_DUMMYNET].active.ptr))) {
		return FALSE;
	}

	return TRUE;
#else
	return FALSE;
#endif /* DUMMYNET */
}

#if DUMMYNET
/*
 * When pf_test_dummynet() returns PF_PASS, the rule matching parameter "rm"
 * remains unchanged, meaning the packet did not match a dummynet rule.
 * when the packet does match a dummynet rule, pf_test_dummynet() returns
 * PF_PASS and zero out the mbuf rule as the packet is effectively siphoned
 * out by dummynet.
 */
static __attribute__((noinline)) int
pf_test_dummynet(struct pf_rule **rm, int direction, struct pfi_kif *kif,
    pbuf_t **pbuf0, struct pf_pdesc *pd, struct ip_fw_args *fwa)
{
	pbuf_t                  *__single pbuf = *pbuf0;
	struct pf_rule          *__single am = NULL;
	struct pf_ruleset       *__single rsm = NULL;
	struct pf_addr          *__single saddr = pd->src, *__single daddr = pd->dst;
	sa_family_t              af = pd->af;
	struct pf_rule          *__single r, *__single a = NULL;
	struct pf_ruleset       *__single ruleset = NULL;
	struct tcphdr           *__single th = pf_pd_get_hdr_tcp(pd);
	u_short                  reason;
	int                      hdrlen = 0;
	int                      tag = -1;
	unsigned int             rtableid = IFSCOPE_NONE;
	int                      asd = 0;
	int                      match = 0;
	u_int8_t                 icmptype = 0, icmpcode = 0;
	struct ip_fw_args       dnflow;
	struct pf_rule          *__single prev_matching_rule = fwa ? fwa->fwa_pf_rule : NULL;
	int                     found_prev_rule = (prev_matching_rule) ? 0 : 1;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	if (!pf_is_dummynet_enabled()) {
		return PF_PASS;
	}

	if (kif->pfik_ifp->if_xflags & IFXF_NO_TRAFFIC_SHAPING) {
		return PF_PASS;
	}

	bzero(&dnflow, sizeof(dnflow));

	hdrlen = 0;

	/* Fragments don't gave protocol headers */
	if (!(pd->flags & PFDESC_IP_FRAG)) {
		switch (pd->proto) {
		case IPPROTO_TCP:
			dnflow.fwa_id.flags = pf_pd_get_hdr_tcp(pd)->th_flags;
			dnflow.fwa_id.dst_port = ntohs(pf_pd_get_hdr_tcp(pd)->th_dport);
			dnflow.fwa_id.src_port = ntohs(pf_pd_get_hdr_tcp(pd)->th_sport);
			hdrlen = sizeof(*th);
			break;
		case IPPROTO_UDP:
			dnflow.fwa_id.dst_port = ntohs(pf_pd_get_hdr_udp(pd)->uh_dport);
			dnflow.fwa_id.src_port = ntohs(pf_pd_get_hdr_udp(pd)->uh_sport);
			hdrlen = sizeof(*pf_pd_get_hdr_udp(pd));
			break;
#if INET
		case IPPROTO_ICMP:
			if (af != AF_INET) {
				break;
			}
			hdrlen = ICMP_MINLEN;
			icmptype = pf_pd_get_hdr_icmp(pd)->icmp_type;
			icmpcode = pf_pd_get_hdr_icmp(pd)->icmp_code;
			break;
#endif /* INET */
		case IPPROTO_ICMPV6:
			if (af != AF_INET6) {
				break;
			}
			hdrlen = sizeof(*pf_pd_get_hdr_icmp6(pd));
			icmptype = pf_pd_get_hdr_icmp6(pd)->icmp6_type;
			icmpcode = pf_pd_get_hdr_icmp6(pd)->icmp6_code;
			break;
		case IPPROTO_GRE:
			if (pd->proto_variant == PF_GRE_PPTP_VARIANT) {
				hdrlen = sizeof(*pf_pd_get_hdr_grev1(pd));
			}
			break;
		case IPPROTO_ESP:
			hdrlen = sizeof(*pf_pd_get_hdr_esp(pd));
			break;
		}
	}

	r = TAILQ_FIRST(pf_main_ruleset.rules[PF_RULESET_DUMMYNET].active.ptr);

	while (r != NULL) {
		r->evaluations++;
		if (pfi_kif_match(r->kif, kif) == r->ifnot) {
			r = r->skip[PF_SKIP_IFP].ptr;
		} else if (r->direction && r->direction != direction) {
			r = r->skip[PF_SKIP_DIR].ptr;
		} else if (r->af && r->af != af) {
			r = r->skip[PF_SKIP_AF].ptr;
		} else if (r->proto && r->proto != pd->proto) {
			r = r->skip[PF_SKIP_PROTO].ptr;
		} else if (PF_MISMATCHAW(&r->src.addr, saddr, af,
		    r->src.neg, kif)) {
			r = r->skip[PF_SKIP_SRC_ADDR].ptr;
		}
		/* tcp/udp only. port_op always 0 in other cases */
		else if (r->proto == pd->proto &&
		    (r->proto == IPPROTO_TCP || r->proto == IPPROTO_UDP) &&
		    ((pd->flags & PFDESC_IP_FRAG) ||
		    ((r->src.xport.range.op &&
		    !pf_match_port(r->src.xport.range.op,
		    r->src.xport.range.port[0], r->src.xport.range.port[1],
		    th->th_sport))))) {
			r = r->skip[PF_SKIP_SRC_PORT].ptr;
		} else if (PF_MISMATCHAW(&r->dst.addr, daddr, af,
		    r->dst.neg, NULL)) {
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		}
		/* tcp/udp only. port_op always 0 in other cases */
		else if (r->proto == pd->proto &&
		    (r->proto == IPPROTO_TCP || r->proto == IPPROTO_UDP) &&
		    r->dst.xport.range.op &&
		    ((pd->flags & PFDESC_IP_FRAG) ||
		    !pf_match_port(r->dst.xport.range.op,
		    r->dst.xport.range.port[0], r->dst.xport.range.port[1],
		    th->th_dport))) {
			r = r->skip[PF_SKIP_DST_PORT].ptr;
		}
		/* icmp only. type always 0 in other cases */
		else if (r->type &&
		    ((pd->flags & PFDESC_IP_FRAG) ||
		    r->type != icmptype + 1)) {
			r = TAILQ_NEXT(r, entries);
		}
		/* icmp only. type always 0 in other cases */
		else if (r->code &&
		    ((pd->flags & PFDESC_IP_FRAG) ||
		    r->code != icmpcode + 1)) {
			r = TAILQ_NEXT(r, entries);
		} else if (r->tos && !(r->tos == pd->tos)) {
			r = TAILQ_NEXT(r, entries);
		} else if (r->rule_flag & PFRULE_FRAGMENT) {
			r = TAILQ_NEXT(r, entries);
		} else if (pd->proto == IPPROTO_TCP &&
		    ((pd->flags & PFDESC_IP_FRAG) ||
		    (r->flagset & th->th_flags) != r->flags)) {
			r = TAILQ_NEXT(r, entries);
		} else if (r->prob && r->prob <= (RandomULong() % (UINT_MAX - 1) + 1)) {
			r = TAILQ_NEXT(r, entries);
		} else if (r->match_tag && !pf_match_tag(r, pd->pf_mtag, &tag)) {
			r = TAILQ_NEXT(r, entries);
		} else {
			/*
			 * Need to go past the previous dummynet matching rule
			 */
			if (r->anchor == NULL) {
				if (found_prev_rule) {
					if (r->tag) {
						tag = r->tag;
					}
					if (PF_RTABLEID_IS_VALID(r->rtableid)) {
						rtableid = r->rtableid;
					}
					match = 1;
					*rm = r;
					am = a;
					rsm = ruleset;
					if ((*rm)->quick) {
						break;
					}
				} else if (r == prev_matching_rule) {
					found_prev_rule = 1;
				}
				r = TAILQ_NEXT(r, entries);
			} else {
				pf_step_into_anchor(&asd, &ruleset,
				    PF_RULESET_DUMMYNET, &r, &a, &match);
			}
		}
		if (r == NULL && pf_step_out_of_anchor(&asd, &ruleset,
		    PF_RULESET_DUMMYNET, &r, &a, &match)) {
			break;
		}
	}
	r = *rm;
	a = am;
	ruleset = rsm;

	if (!match) {
		return PF_PASS;
	}

	REASON_SET(&reason, PFRES_DUMMYNET);

	if (r->log) {
		PFLOG_PACKET(kif, h, pbuf, af, direction, reason, r,
		    a, ruleset, pd);
	}

	if (r->action == PF_NODUMMYNET) {
		int dirndx = (direction == PF_OUT);

		r->packets[dirndx]++;
		r->bytes[dirndx] += pd->tot_len;

		return PF_PASS;
	}
	if (pf_tag_packet(pbuf, pd->pf_mtag, tag, rtableid, pd)) {
		REASON_SET(&reason, PFRES_MEMORY);

		return PF_DROP;
	}

	if (r->dnpipe && ip_dn_io_ptr != NULL) {
		struct mbuf *m;
		int dirndx = (direction == PF_OUT);

		r->packets[dirndx]++;
		r->bytes[dirndx] += pd->tot_len;

		dnflow.fwa_cookie = r->dnpipe;
		dnflow.fwa_pf_rule = r;
		dnflow.fwa_id.proto = pd->proto;
		dnflow.fwa_flags = r->dntype;
		switch (af) {
		case AF_INET:
			dnflow.fwa_id.addr_type = 4;
			dnflow.fwa_id.src_ip = ntohl(saddr->v4addr.s_addr);
			dnflow.fwa_id.dst_ip = ntohl(daddr->v4addr.s_addr);
			break;
		case AF_INET6:
			dnflow.fwa_id.addr_type = 6;
			dnflow.fwa_id.src_ip6 = saddr->v6addr;
			dnflow.fwa_id.dst_ip6 = saddr->v6addr;
			break;
		}

		if (fwa != NULL) {
			dnflow.fwa_oif = fwa->fwa_oif;
			dnflow.fwa_oflags = fwa->fwa_oflags;
			/*
			 * Note that fwa_ro, fwa_dst and fwa_ipoa are
			 * actually in a union so the following does work
			 * for both IPv4 and IPv6
			 */
			dnflow.fwa_ro = fwa->fwa_ro;
			dnflow.fwa_dst = fwa->fwa_dst;
			dnflow.fwa_ipoa = fwa->fwa_ipoa;
			dnflow.fwa_ro6_pmtu = fwa->fwa_ro6_pmtu;
			dnflow.fwa_origifp = fwa->fwa_origifp;
			dnflow.fwa_mtu = fwa->fwa_mtu;
			dnflow.fwa_unfragpartlen = fwa->fwa_unfragpartlen;
			dnflow.fwa_exthdrs = fwa->fwa_exthdrs;
		}

		if (af == AF_INET) {
			struct ip *__single iphdr = pbuf->pb_data;
			NTOHS(iphdr->ip_len);
			NTOHS(iphdr->ip_off);
		}
		/*
		 * Don't need to unlock pf_lock as NET_THREAD_HELD_PF
		 * allows for recursive behavior
		 */
		m = pbuf_to_mbuf(pbuf, TRUE);
		if (m != NULL) {
			ip_dn_io_ptr(m,
			    dnflow.fwa_cookie, (af == AF_INET) ?
			    ((direction == PF_IN) ? DN_TO_IP_IN : DN_TO_IP_OUT) :
			    ((direction == PF_IN) ? DN_TO_IP6_IN : DN_TO_IP6_OUT),
			    &dnflow);
		}

		/*
		 * The packet is siphoned out by dummynet so return a NULL
		 * pbuf so the caller can still return success.
		 */
		*pbuf0 = NULL;

		return PF_PASS;
	}

	return PF_PASS;
}
#endif /* DUMMYNET */

static __attribute__((noinline)) int
pf_test_fragment(struct pf_rule **rm, int direction, struct pfi_kif *kif,
    pbuf_t *pbuf, void *h, struct pf_pdesc *pd, struct pf_rule **am,
    struct pf_ruleset **rsm)
{
#pragma unused(h)
	struct pf_rule          *__single r, *__single a = NULL;
	struct pf_ruleset       *__single ruleset = NULL;
	sa_family_t              af = pd->af;
	u_short                  reason;
	int                      tag = -1;
	int                      asd = 0;
	int                      match = 0;

	r = TAILQ_FIRST(pf_main_ruleset.rules[PF_RULESET_FILTER].active.ptr);
	while (r != NULL) {
		r->evaluations++;
		if (pfi_kif_match(r->kif, kif) == r->ifnot) {
			r = r->skip[PF_SKIP_IFP].ptr;
		} else if (r->direction && r->direction != direction) {
			r = r->skip[PF_SKIP_DIR].ptr;
		} else if (r->af && r->af != af) {
			r = r->skip[PF_SKIP_AF].ptr;
		} else if (r->proto && r->proto != pd->proto) {
			r = r->skip[PF_SKIP_PROTO].ptr;
		} else if (PF_MISMATCHAW(&r->src.addr, pd->src, af,
		    r->src.neg, kif)) {
			r = r->skip[PF_SKIP_SRC_ADDR].ptr;
		} else if (PF_MISMATCHAW(&r->dst.addr, pd->dst, af,
		    r->dst.neg, NULL)) {
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		} else if ((r->rule_flag & PFRULE_TOS) && r->tos &&
		    !(r->tos & pd->tos)) {
			r = TAILQ_NEXT(r, entries);
		} else if ((r->rule_flag & PFRULE_DSCP) && r->tos &&
		    !(r->tos & (pd->tos & DSCP_MASK))) {
			r = TAILQ_NEXT(r, entries);
		} else if ((r->rule_flag & PFRULE_SC) && r->tos &&
		    ((r->tos & SCIDX_MASK) != pd->sc)) {
			r = TAILQ_NEXT(r, entries);
		} else if (r->os_fingerprint != PF_OSFP_ANY) {
			r = TAILQ_NEXT(r, entries);
		} else if (pd->proto == IPPROTO_UDP &&
		    (r->src.xport.range.op || r->dst.xport.range.op)) {
			r = TAILQ_NEXT(r, entries);
		} else if (pd->proto == IPPROTO_TCP &&
		    (r->src.xport.range.op || r->dst.xport.range.op ||
		    r->flagset)) {
			r = TAILQ_NEXT(r, entries);
		} else if ((pd->proto == IPPROTO_ICMP ||
		    pd->proto == IPPROTO_ICMPV6) &&
		    (r->type || r->code)) {
			r = TAILQ_NEXT(r, entries);
		} else if (r->prob && r->prob <= (RandomULong() % (UINT_MAX - 1) + 1)) {
			r = TAILQ_NEXT(r, entries);
		} else if (r->match_tag && !pf_match_tag(r, pd->pf_mtag, &tag)) {
			r = TAILQ_NEXT(r, entries);
		} else {
			if (r->anchor == NULL) {
				match = 1;
				*rm = r;
				*am = a;
				*rsm = ruleset;
				if ((*rm)->quick) {
					break;
				}
				r = TAILQ_NEXT(r, entries);
			} else {
				pf_step_into_anchor(&asd, &ruleset,
				    PF_RULESET_FILTER, &r, &a, &match);
			}
		}
		if (r == NULL && pf_step_out_of_anchor(&asd, &ruleset,
		    PF_RULESET_FILTER, &r, &a, &match)) {
			break;
		}
	}
	r = *rm;
	a = *am;
	ruleset = *rsm;

	REASON_SET(&reason, PFRES_MATCH);

	if (r->log) {
		PFLOG_PACKET(kif, h, pbuf, af, direction, reason, r, a, ruleset,
		    pd);
	}

	if (r->action != PF_PASS) {
		return PF_DROP;
	}

	if (pf_tag_packet(pbuf, pd->pf_mtag, tag, -1, NULL)) {
		REASON_SET(&reason, PFRES_MEMORY);
		return PF_DROP;
	}

	return PF_PASS;
}

static __attribute__((noinline)) void
pf_pptp_handler(struct pf_state *s, int direction, int off,
    struct pf_pdesc *pd, struct pfi_kif *kif)
{
#pragma unused(direction)
	struct tcphdr *__single th;
	struct pf_pptp_state *__single pptps;
	struct pf_pptp_ctrl_msg cm;
	size_t plen, tlen;
	struct pf_state *__single gs;
	u_int16_t ct;
	u_int16_t *__single pac_call_id;
	u_int16_t *__single pns_call_id;
	u_int16_t *__single spoof_call_id;
	u_int8_t *__single pac_state;
	u_int8_t *__single pns_state;
	enum { PF_PPTP_PASS, PF_PPTP_INSERT_GRE, PF_PPTP_REMOVE_GRE } op;
	pbuf_t *__single pbuf;
	struct pf_state_key *__single sk;
	struct pf_state_key *__single gsk;
	struct pf_app_state *__single gas;

	sk = s->state_key;
	pptps = &sk->app_state->u.pptp;
	gs = pptps->grev1_state;

	if (gs) {
		gs->expire = pf_time_second();
	}

	pbuf = pd->mp;
	plen = min(sizeof(cm), pbuf->pb_packet_len - off);
	if (plen < PF_PPTP_CTRL_MSG_MINSIZE) {
		return;
	}
	tlen = plen - PF_PPTP_CTRL_MSG_MINSIZE;
	pbuf_copy_data(pbuf, off, plen, &cm, sizeof(cm));

	if (ntohl(cm.hdr.magic) != PF_PPTP_MAGIC_NUMBER) {
		return;
	}
	if (ntohs(cm.hdr.type) != 1) {
		return;
	}

#define TYPE_LEN_CHECK(_type, _name)                            \
	case PF_PPTP_CTRL_TYPE_##_type:                         \
	        if (tlen < sizeof(struct pf_pptp_ctrl_##_name)) \
	                return;                                 \
	        break;

	switch (cm.ctrl.type) {
		TYPE_LEN_CHECK(START_REQ, start_req);
		TYPE_LEN_CHECK(START_RPY, start_rpy);
		TYPE_LEN_CHECK(STOP_REQ, stop_req);
		TYPE_LEN_CHECK(STOP_RPY, stop_rpy);
		TYPE_LEN_CHECK(ECHO_REQ, echo_req);
		TYPE_LEN_CHECK(ECHO_RPY, echo_rpy);
		TYPE_LEN_CHECK(CALL_OUT_REQ, call_out_req);
		TYPE_LEN_CHECK(CALL_OUT_RPY, call_out_rpy);
		TYPE_LEN_CHECK(CALL_IN_1ST, call_in_1st);
		TYPE_LEN_CHECK(CALL_IN_2ND, call_in_2nd);
		TYPE_LEN_CHECK(CALL_IN_3RD, call_in_3rd);
		TYPE_LEN_CHECK(CALL_CLR, call_clr);
		TYPE_LEN_CHECK(CALL_DISC, call_disc);
		TYPE_LEN_CHECK(ERROR, error);
		TYPE_LEN_CHECK(SET_LINKINFO, set_linkinfo);
	default:
		return;
	}
#undef TYPE_LEN_CHECK

	if (!gs) {
		gs = pool_get(&pf_state_pl, PR_WAITOK);
		if (!gs) {
			return;
		}

		memcpy(gs, s, sizeof(*gs));

		memset(&gs->entry_id, 0, sizeof(gs->entry_id));
		memset(&gs->entry_list, 0, sizeof(gs->entry_list));

		TAILQ_INIT(&gs->unlink_hooks);
		gs->rt_kif = NULL;
		gs->creation = 0;
		gs->pfsync_time = 0;
		gs->packets[0] = gs->packets[1] = 0;
		gs->bytes[0] = gs->bytes[1] = 0;
		gs->timeout = PFTM_UNLINKED;
		gs->id = gs->creatorid = 0;
		gs->src.state = gs->dst.state = PFGRE1S_NO_TRAFFIC;
		gs->src.scrub = gs->dst.scrub = 0;

		gas = pool_get(&pf_app_state_pl, PR_NOWAIT);
		if (!gas) {
			pool_put(&pf_state_pl, gs);
			return;
		}

		gsk = pf_alloc_state_key(gs, NULL);
		if (!gsk) {
			pool_put(&pf_app_state_pl, gas);
			pool_put(&pf_state_pl, gs);
			return;
		}

		memcpy(&gsk->lan, &sk->lan, sizeof(gsk->lan));
		memcpy(&gsk->gwy, &sk->gwy, sizeof(gsk->gwy));
		memcpy(&gsk->ext_lan, &sk->ext_lan, sizeof(gsk->ext_lan));
		memcpy(&gsk->ext_gwy, &sk->ext_gwy, sizeof(gsk->ext_gwy));
		gsk->af_lan = sk->af_lan;
		gsk->af_gwy = sk->af_gwy;
		gsk->proto = IPPROTO_GRE;
		gsk->proto_variant = PF_GRE_PPTP_VARIANT;
		gsk->app_state = gas;
		gsk->lan.xport.call_id = 0;
		gsk->gwy.xport.call_id = 0;
		gsk->ext_lan.xport.call_id = 0;
		gsk->ext_gwy.xport.call_id = 0;
		ASSERT(gsk->flowsrc == FLOWSRC_PF);
		ASSERT(gsk->flowhash != 0);
		memset(gas, 0, sizeof(*gas));
		gas->u.grev1.pptp_state = s;
		STATE_INC_COUNTERS(gs);
		pptps->grev1_state = gs;
		(void) hook_establish(&gs->unlink_hooks, 0,
		    (hook_fn_t) pf_grev1_unlink, gs);
	} else {
		gsk = gs->state_key;
	}

	switch (sk->direction) {
	case PF_IN:
		pns_call_id = &gsk->ext_lan.xport.call_id;
		pns_state = &gs->dst.state;
		pac_call_id = &gsk->lan.xport.call_id;
		pac_state = &gs->src.state;
		break;

	case PF_OUT:
		pns_call_id = &gsk->lan.xport.call_id;
		pns_state = &gs->src.state;
		pac_call_id = &gsk->ext_lan.xport.call_id;
		pac_state = &gs->dst.state;
		break;

	default:
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_pptp_handler: bad directional!\n"));
		return;
	}

	spoof_call_id = 0;
	op = PF_PPTP_PASS;

	ct = ntohs(cm.ctrl.type);

	switch (ct) {
	case PF_PPTP_CTRL_TYPE_CALL_OUT_REQ:
		*pns_call_id = cm.msg.call_out_req.call_id;
		*pns_state = PFGRE1S_INITIATING;
		if (s->nat_rule.ptr && pns_call_id == &gsk->lan.xport.call_id) {
			spoof_call_id = &cm.msg.call_out_req.call_id;
		}
		break;

	case PF_PPTP_CTRL_TYPE_CALL_OUT_RPY:
		*pac_call_id = cm.msg.call_out_rpy.call_id;
		if (s->nat_rule.ptr) {
			spoof_call_id =
			    (pac_call_id == &gsk->lan.xport.call_id) ?
			    &cm.msg.call_out_rpy.call_id :
			    &cm.msg.call_out_rpy.peer_call_id;
		}
		if (gs->timeout == PFTM_UNLINKED) {
			*pac_state = PFGRE1S_INITIATING;
			op = PF_PPTP_INSERT_GRE;
		}
		break;

	case PF_PPTP_CTRL_TYPE_CALL_IN_1ST:
		*pns_call_id = cm.msg.call_in_1st.call_id;
		*pns_state = PFGRE1S_INITIATING;
		if (s->nat_rule.ptr && pns_call_id == &gsk->lan.xport.call_id) {
			spoof_call_id = &cm.msg.call_in_1st.call_id;
		}
		break;

	case PF_PPTP_CTRL_TYPE_CALL_IN_2ND:
		*pac_call_id = cm.msg.call_in_2nd.call_id;
		*pac_state = PFGRE1S_INITIATING;
		if (s->nat_rule.ptr) {
			spoof_call_id =
			    (pac_call_id == &gsk->lan.xport.call_id) ?
			    &cm.msg.call_in_2nd.call_id :
			    &cm.msg.call_in_2nd.peer_call_id;
		}
		break;

	case PF_PPTP_CTRL_TYPE_CALL_IN_3RD:
		if (s->nat_rule.ptr && pns_call_id == &gsk->lan.xport.call_id) {
			spoof_call_id = &cm.msg.call_in_3rd.call_id;
		}
		if (cm.msg.call_in_3rd.call_id != *pns_call_id) {
			break;
		}
		if (gs->timeout == PFTM_UNLINKED) {
			op = PF_PPTP_INSERT_GRE;
		}
		break;

	case PF_PPTP_CTRL_TYPE_CALL_CLR:
		if (cm.msg.call_clr.call_id != *pns_call_id) {
			op = PF_PPTP_REMOVE_GRE;
		}
		break;

	case PF_PPTP_CTRL_TYPE_CALL_DISC:
		if (cm.msg.call_clr.call_id != *pac_call_id) {
			op = PF_PPTP_REMOVE_GRE;
		}
		break;

	case PF_PPTP_CTRL_TYPE_ERROR:
		if (s->nat_rule.ptr && pns_call_id == &gsk->lan.xport.call_id) {
			spoof_call_id = &cm.msg.error.peer_call_id;
		}
		break;

	case PF_PPTP_CTRL_TYPE_SET_LINKINFO:
		if (s->nat_rule.ptr && pac_call_id == &gsk->lan.xport.call_id) {
			spoof_call_id = &cm.msg.set_linkinfo.peer_call_id;
		}
		break;

	default:
		op = PF_PPTP_PASS;
		break;
	}

	if (!gsk->gwy.xport.call_id && gsk->lan.xport.call_id) {
		gsk->gwy.xport.call_id = gsk->lan.xport.call_id;
		if (spoof_call_id) {
			u_int16_t call_id = 0;
			int n = 0;
			struct pf_state_key_cmp key;

			key.af_gwy = gsk->af_gwy;
			key.proto = IPPROTO_GRE;
			key.proto_variant = PF_GRE_PPTP_VARIANT;
			PF_ACPY(&key.gwy.addr, &gsk->gwy.addr, key.af_gwy);
			PF_ACPY(&key.ext_gwy.addr, &gsk->ext_gwy.addr, key.af_gwy);
			key.gwy.xport.call_id = gsk->gwy.xport.call_id;
			key.ext_gwy.xport.call_id = gsk->ext_gwy.xport.call_id;
			do {
				call_id = htonl(random());
			} while (!call_id);

			while (pf_find_state_all(&key, PF_IN, 0)) {
				call_id = ntohs(call_id);
				--call_id;
				if (--call_id == 0) {
					call_id = 0xffff;
				}
				call_id = htons(call_id);

				key.gwy.xport.call_id = call_id;

				if (++n > 65535) {
					DPFPRINTF(PF_DEBUG_URGENT,
					    ("pf_pptp_handler: failed to spoof "
					    "call id\n"));
					key.gwy.xport.call_id = 0;
					break;
				}
			}

			gsk->gwy.xport.call_id = call_id;
		}
	}

	th = pf_pd_get_hdr_tcp(pd);

	if (spoof_call_id && gsk->lan.xport.call_id != gsk->gwy.xport.call_id) {
		if (*spoof_call_id == gsk->gwy.xport.call_id) {
			*spoof_call_id = gsk->lan.xport.call_id;
			th->th_sum = pf_cksum_fixup(th->th_sum,
			    gsk->gwy.xport.call_id, gsk->lan.xport.call_id, 0);
		} else {
			*spoof_call_id = gsk->gwy.xport.call_id;
			th->th_sum = pf_cksum_fixup(th->th_sum,
			    gsk->lan.xport.call_id, gsk->gwy.xport.call_id, 0);
		}

		if (pf_lazy_makewritable(pd, pbuf, off + plen) == NULL) {
			pptps->grev1_state = NULL;
			STATE_DEC_COUNTERS(gs);
			pool_put(&pf_state_pl, gs);
			return;
		}
		pbuf_copy_back(pbuf, off, plen, &cm, sizeof(cm));
	}

	switch (op) {
	case PF_PPTP_REMOVE_GRE:
		gs->timeout = PFTM_PURGE;
		gs->src.state = gs->dst.state = PFGRE1S_NO_TRAFFIC;
		gsk->lan.xport.call_id = 0;
		gsk->gwy.xport.call_id = 0;
		gsk->ext_lan.xport.call_id = 0;
		gsk->ext_gwy.xport.call_id = 0;
		gs->id = gs->creatorid = 0;
		break;

	case PF_PPTP_INSERT_GRE:
		gs->creation = pf_time_second();
		gs->expire = pf_time_second();
		gs->timeout = PFTM_TCP_ESTABLISHED;
		if (gs->src_node != NULL) {
			++gs->src_node->states;
			VERIFY(gs->src_node->states != 0);
		}
		if (gs->nat_src_node != NULL) {
			++gs->nat_src_node->states;
			VERIFY(gs->nat_src_node->states != 0);
		}
		pf_set_rt_ifp(gs, &sk->lan.addr, sk->af_lan);
		if (pf_insert_state(BOUND_IFACE(s->rule.ptr, kif), gs)) {
			/*
			 * <jhw@apple.com>
			 * FIX ME: insertion can fail when multiple PNS
			 * behind the same NAT open calls to the same PAC
			 * simultaneously because spoofed call ID numbers
			 * are chosen before states are inserted.  This is
			 * hard to fix and happens infrequently enough that
			 * users will normally try again and this ALG will
			 * succeed.  Failures are expected to be rare enough
			 * that fixing this is a low priority.
			 */
			pptps->grev1_state = NULL;
			pd->lmw = -1;   /* Force PF_DROP on PFRES_MEMORY */
			pf_src_tree_remove_state(gs);
			STATE_DEC_COUNTERS(gs);
			pool_put(&pf_state_pl, gs);
			DPFPRINTF(PF_DEBUG_URGENT, ("pf_pptp_handler: error "
			    "inserting GREv1 state.\n"));
		}
		break;

	default:
		break;
	}
}

static __attribute__((noinline)) void
pf_pptp_unlink(struct pf_state *s)
{
	struct pf_app_state *as = s->state_key->app_state;
	struct pf_state *grev1s = as->u.pptp.grev1_state;

	if (grev1s) {
		struct pf_app_state *gas = grev1s->state_key->app_state;

		if (grev1s->timeout < PFTM_MAX) {
			grev1s->timeout = PFTM_PURGE;
		}
		gas->u.grev1.pptp_state = NULL;
		as->u.pptp.grev1_state = NULL;
	}
}

static __attribute__((noinline)) void
pf_grev1_unlink(struct pf_state *s)
{
	struct pf_app_state *as = s->state_key->app_state;
	struct pf_state *pptps = as->u.grev1.pptp_state;

	if (pptps) {
		struct pf_app_state *pas = pptps->state_key->app_state;

		pas->u.pptp.grev1_state = NULL;
		as->u.grev1.pptp_state = NULL;
	}
}

static int
pf_ike_compare(struct pf_app_state *a, struct pf_app_state *b)
{
	int64_t d = a->u.ike.cookie - b->u.ike.cookie;
	return (d > 0) ? 1 : ((d < 0) ? -1 : 0);
}

static int
pf_do_nat64(struct pf_state_key *sk, struct pf_pdesc *pd, pbuf_t *pbuf,
    int off)
{
	if (pd->af == AF_INET) {
		if (pd->af != sk->af_lan) {
			pd->ndaddr = sk->lan.addr;
			pd->naddr = sk->ext_lan.addr;
		} else {
			pd->naddr = sk->gwy.addr;
			pd->ndaddr = sk->ext_gwy.addr;
		}
		return pf_nat64_ipv4(pbuf, off, pd);
	} else if (pd->af == AF_INET6) {
		if (pd->af != sk->af_lan) {
			pd->ndaddr = sk->lan.addr;
			pd->naddr = sk->ext_lan.addr;
		} else {
			pd->naddr = sk->gwy.addr;
			pd->ndaddr = sk->ext_gwy.addr;
		}
		return pf_nat64_ipv6(pbuf, off, pd);
	}
	return PF_DROP;
}

static __attribute__((noinline)) int
pf_test_state_tcp(struct pf_state **state, int direction, struct pfi_kif *kif,
    pbuf_t *pbuf, int off, void *h, struct pf_pdesc *pd,
    u_short *reason)
{
#pragma unused(h)
	struct pf_state_key_cmp  key;
	struct tcphdr           *__single th = pf_pd_get_hdr_tcp(pd);
	u_int16_t                win = ntohs(th->th_win);
	u_int32_t                ack, end, seq, orig_seq;
	u_int8_t                 sws, dws;
	int                      ackskew;
	int                      copyback = 0;
	struct pf_state_peer    *src, *dst;
	struct pf_state_key     *sk;

	key.app_state = 0;
	key.proto = IPPROTO_TCP;
	key.af_lan = key.af_gwy = pd->af;

	/*
	 * For NAT64 the first time rule search and state creation
	 * is done on the incoming side only.
	 * Once the state gets created, NAT64's LAN side (ipv6) will
	 * not be able to find the state in ext-gwy tree as that normally
	 * is intended to be looked up for incoming traffic from the
	 * WAN side.
	 * Therefore to handle NAT64 case we init keys here for both
	 * lan-ext as well as ext-gwy trees.
	 * In the state lookup we attempt a lookup on both trees if
	 * first one does not return any result and return a match if
	 * the match state's was created by NAT64 rule.
	 */
	PF_ACPY(&key.ext_gwy.addr, pd->src, key.af_gwy);
	PF_ACPY(&key.gwy.addr, pd->dst, key.af_gwy);
	key.ext_gwy.xport.port = th->th_sport;
	key.gwy.xport.port = th->th_dport;

	PF_ACPY(&key.lan.addr, pd->src, key.af_lan);
	PF_ACPY(&key.ext_lan.addr, pd->dst, key.af_lan);
	key.lan.xport.port = th->th_sport;
	key.ext_lan.xport.port = th->th_dport;

	STATE_LOOKUP();

	sk = (*state)->state_key;
	/*
	 * In case of NAT64 the translation is first applied on the LAN
	 * side. Therefore for stack's address family comparison
	 * we use sk->af_lan.
	 */
	if ((direction == sk->direction) && (pd->af == sk->af_lan)) {
		src = &(*state)->src;
		dst = &(*state)->dst;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

	if (src->state == PF_TCPS_PROXY_SRC) {
		if (direction != sk->direction) {
			REASON_SET(reason, PFRES_SYNPROXY);
			return PF_SYNPROXY_DROP;
		}
		if (th->th_flags & TH_SYN) {
			if (ntohl(th->th_seq) != src->seqlo) {
				REASON_SET(reason, PFRES_SYNPROXY);
				return PF_DROP;
			}
			pf_send_tcp((*state)->rule.ptr, pd->af, pd->dst,
			    pd->src, th->th_dport, th->th_sport,
			    src->seqhi, ntohl(th->th_seq) + 1,
			    TH_SYN | TH_ACK, 0, src->mss, 0, 1,
			    0, NULL, NULL);
			REASON_SET(reason, PFRES_SYNPROXY);
			return PF_SYNPROXY_DROP;
		} else if (!(th->th_flags & TH_ACK) ||
		    (ntohl(th->th_ack) != src->seqhi + 1) ||
		    (ntohl(th->th_seq) != src->seqlo + 1)) {
			REASON_SET(reason, PFRES_SYNPROXY);
			return PF_DROP;
		} else if ((*state)->src_node != NULL &&
		    pf_src_connlimit(state)) {
			REASON_SET(reason, PFRES_SRCLIMIT);
			return PF_DROP;
		} else {
			src->state = PF_TCPS_PROXY_DST;
		}
	}
	if (src->state == PF_TCPS_PROXY_DST) {
		struct pf_state_host *psrc, *pdst;

		if (direction == PF_OUT) {
			psrc = &sk->gwy;
			pdst = &sk->ext_gwy;
		} else {
			psrc = &sk->ext_lan;
			pdst = &sk->lan;
		}
		if (direction == sk->direction) {
			if (((th->th_flags & (TH_SYN | TH_ACK)) != TH_ACK) ||
			    (ntohl(th->th_ack) != src->seqhi + 1) ||
			    (ntohl(th->th_seq) != src->seqlo + 1)) {
				REASON_SET(reason, PFRES_SYNPROXY);
				return PF_DROP;
			}
			src->max_win = MAX(ntohs(th->th_win), 1);
			if (dst->seqhi == 1) {
				dst->seqhi = htonl(random());
			}
			pf_send_tcp((*state)->rule.ptr, pd->af, &psrc->addr,
			    &pdst->addr, psrc->xport.port, pdst->xport.port,
			    dst->seqhi, 0, TH_SYN, 0,
			    src->mss, 0, 0, (*state)->tag, NULL, NULL);
			REASON_SET(reason, PFRES_SYNPROXY);
			return PF_SYNPROXY_DROP;
		} else if (((th->th_flags & (TH_SYN | TH_ACK)) !=
		    (TH_SYN | TH_ACK)) ||
		    (ntohl(th->th_ack) != dst->seqhi + 1)) {
			REASON_SET(reason, PFRES_SYNPROXY);
			return PF_DROP;
		} else {
			dst->max_win = MAX(ntohs(th->th_win), 1);
			dst->seqlo = ntohl(th->th_seq);
			pf_send_tcp((*state)->rule.ptr, pd->af, pd->dst,
			    pd->src, th->th_dport, th->th_sport,
			    ntohl(th->th_ack), ntohl(th->th_seq) + 1,
			    TH_ACK, src->max_win, 0, 0, 0,
			    (*state)->tag, NULL, NULL);
			pf_send_tcp((*state)->rule.ptr, pd->af, &psrc->addr,
			    &pdst->addr, psrc->xport.port, pdst->xport.port,
			    src->seqhi + 1, src->seqlo + 1,
			    TH_ACK, dst->max_win, 0, 0, 1,
			    0, NULL, NULL);
			src->seqdiff = dst->seqhi -
			    src->seqlo;
			dst->seqdiff = src->seqhi -
			    dst->seqlo;
			src->seqhi = src->seqlo +
			    dst->max_win;
			dst->seqhi = dst->seqlo +
			    src->max_win;
			src->wscale = dst->wscale = 0;
			src->state = dst->state =
			    TCPS_ESTABLISHED;
			REASON_SET(reason, PFRES_SYNPROXY);
			return PF_SYNPROXY_DROP;
		}
	}

	if (((th->th_flags & (TH_SYN | TH_ACK)) == TH_SYN) &&
	    dst->state >= TCPS_FIN_WAIT_2 &&
	    src->state >= TCPS_FIN_WAIT_2) {
		if (pf_status.debug >= PF_DEBUG_MISC) {
			printf("pf: state reuse ");
			pf_print_state(*state);
			pf_print_flags(th->th_flags);
			printf("\n");
		}
		/* XXX make sure it's the same direction ?? */
		src->state = dst->state = TCPS_CLOSED;
		pf_unlink_state(*state);
		*state = NULL;
		return PF_DROP;
	}

	if ((th->th_flags & TH_SYN) == 0) {
		sws = (src->wscale & PF_WSCALE_FLAG) ?
		    (src->wscale & PF_WSCALE_MASK) : TCP_MAX_WINSHIFT;
		dws = (dst->wscale & PF_WSCALE_FLAG) ?
		    (dst->wscale & PF_WSCALE_MASK) : TCP_MAX_WINSHIFT;
	} else {
		sws = dws = 0;
	}

	/*
	 * Sequence tracking algorithm from Guido van Rooij's paper:
	 *   http://www.madison-gurkha.com/publications/tcp_filtering/
	 *	tcp_filtering.ps
	 */

	orig_seq = seq = ntohl(th->th_seq);
	if (src->seqlo == 0) {
		/* First packet from this end. Set its state */

		if ((pd->flags & PFDESC_TCP_NORM || dst->scrub) &&
		    src->scrub == NULL) {
			if (pf_normalize_tcp_init(pbuf, off, pd, th, src, dst)) {
				REASON_SET(reason, PFRES_MEMORY);
				return PF_DROP;
			}
		}

		/* Deferred generation of sequence number modulator */
		if (dst->seqdiff && !src->seqdiff) {
			/* use random iss for the TCP server */
			while ((src->seqdiff = random() - seq) == 0) {
				;
			}
			ack = ntohl(th->th_ack) - dst->seqdiff;
			pf_change_a(&th->th_seq, &th->th_sum, htonl(seq +
			    src->seqdiff), 0);
			pf_change_a(&th->th_ack, &th->th_sum, htonl(ack), 0);
			copyback = off + sizeof(*th);
		} else {
			ack = ntohl(th->th_ack);
		}

		end = seq + pd->p_len;
		if (th->th_flags & TH_SYN) {
			end++;
			if (dst->wscale & PF_WSCALE_FLAG) {
				src->wscale = pf_get_wscale(pbuf, off,
				    th->th_off, pd->af);
				if (src->wscale & PF_WSCALE_FLAG) {
					/*
					 * Remove scale factor from initial
					 * window
					 */
					sws = src->wscale & PF_WSCALE_MASK;
					win = ((u_int32_t)win + (1 << sws) - 1)
					    >> sws;
					dws = dst->wscale & PF_WSCALE_MASK;
				} else {
					/*
					 * Window scale negotiation has failed,
					 * therefore we must restore the window
					 * scale in the state record that we
					 * optimistically removed in
					 * pf_test_rule().  Care is required to
					 * prevent arithmetic overflow from
					 * zeroing the window when it's
					 * truncated down to 16-bits.
					 */
					u_int32_t max_win = dst->max_win;
					max_win <<=
					    dst->wscale & PF_WSCALE_MASK;
					dst->max_win = MIN(0xffff, max_win);
					/* in case of a retrans SYN|ACK */
					dst->wscale = 0;
				}
			}
		}
		if (th->th_flags & TH_FIN) {
			end++;
		}

		src->seqlo = seq;
		if (src->state < TCPS_SYN_SENT) {
			src->state = TCPS_SYN_SENT;
		}

		/*
		 * May need to slide the window (seqhi may have been set by
		 * the crappy stack check or if we picked up the connection
		 * after establishment)
		 */
		if (src->seqhi == 1 ||
		    SEQ_GEQ(end + MAX(1, (u_int32_t)dst->max_win << dws),
		    src->seqhi)) {
			src->seqhi = end + MAX(1, (u_int32_t)dst->max_win << dws);
		}
		if (win > src->max_win) {
			src->max_win = win;
		}
	} else {
		ack = ntohl(th->th_ack) - dst->seqdiff;
		if (src->seqdiff) {
			/* Modulate sequence numbers */
			pf_change_a(&th->th_seq, &th->th_sum, htonl(seq +
			    src->seqdiff), 0);
			pf_change_a(&th->th_ack, &th->th_sum, htonl(ack), 0);
			copyback = off + sizeof(*th);
		}
		end = seq + pd->p_len;
		if (th->th_flags & TH_SYN) {
			end++;
		}
		if (th->th_flags & TH_FIN) {
			end++;
		}
	}

	if ((th->th_flags & TH_ACK) == 0) {
		/* Let it pass through the ack skew check */
		ack = dst->seqlo;
	} else if ((ack == 0 &&
	    (th->th_flags & (TH_ACK | TH_RST)) == (TH_ACK | TH_RST)) ||
	    /* broken tcp stacks do not set ack */
	    (dst->state < TCPS_SYN_SENT)) {
		/*
		 * Many stacks (ours included) will set the ACK number in an
		 * FIN|ACK if the SYN times out -- no sequence to ACK.
		 */
		ack = dst->seqlo;
	}

	if (seq == end) {
		/* Ease sequencing restrictions on no data packets */
		seq = src->seqlo;
		end = seq;
	}

	ackskew = dst->seqlo - ack;


	/*
	 * Need to demodulate the sequence numbers in any TCP SACK options
	 * (Selective ACK). We could optionally validate the SACK values
	 * against the current ACK window, either forwards or backwards, but
	 * I'm not confident that SACK has been implemented properly
	 * everywhere. It wouldn't surprise me if several stacks accidently
	 * SACK too far backwards of previously ACKed data. There really aren't
	 * any security implications of bad SACKing unless the target stack
	 * doesn't validate the option length correctly. Someone trying to
	 * spoof into a TCP connection won't bother blindly sending SACK
	 * options anyway.
	 */
	if (dst->seqdiff && (th->th_off << 2) > (int)sizeof(struct tcphdr)) {
		copyback = pf_modulate_sack(pbuf, off, pd, th, dst);
		if (copyback == -1) {
			REASON_SET(reason, PFRES_MEMORY);
			return PF_DROP;
		}

		pbuf = pd->mp;  // XXXSCW: Why?
	}


#define MAXACKWINDOW (0xffff + 1500)    /* 1500 is an arbitrary fudge factor */
	if (SEQ_GEQ(src->seqhi, end) &&
	    /* Last octet inside other's window space */
	    SEQ_GEQ(seq, src->seqlo - ((u_int32_t)dst->max_win << dws)) &&
	    /* Retrans: not more than one window back */
	    (ackskew >= -MAXACKWINDOW) &&
	    /* Acking not more than one reassembled fragment backwards */
	    (ackskew <= (MAXACKWINDOW << sws)) &&
	    /* Acking not more than one window forward */
	    ((th->th_flags & TH_RST) == 0 || orig_seq == src->seqlo ||
	    (orig_seq == src->seqlo + 1) || (orig_seq + 1 == src->seqlo) ||
	    (pd->flags & PFDESC_IP_REAS) == 0)) {
		/* Require an exact/+1 sequence match on resets when possible */

		if (dst->scrub || src->scrub) {
			if (pf_normalize_tcp_stateful(pbuf, off, pd, reason, th,
			    *state, src, dst, &copyback)) {
				return PF_DROP;
			}

			pbuf = pd->mp;  // XXXSCW: Why?
		}

		/* update max window */
		if (src->max_win < win) {
			src->max_win = win;
		}
		/* synchronize sequencing */
		if (SEQ_GT(end, src->seqlo)) {
			src->seqlo = end;
		}
		/* slide the window of what the other end can send */
		if (SEQ_GEQ(ack + ((u_int32_t)win << sws), dst->seqhi)) {
			dst->seqhi = ack + MAX(((u_int32_t)win << sws), 1);
		}

		/* update states */
		if (th->th_flags & TH_SYN) {
			if (src->state < TCPS_SYN_SENT) {
				src->state = TCPS_SYN_SENT;
			}
		}
		if (th->th_flags & TH_FIN) {
			if (src->state < TCPS_CLOSING) {
				src->state = TCPS_CLOSING;
			}
		}
		if (th->th_flags & TH_ACK) {
			if (dst->state == TCPS_SYN_SENT) {
				dst->state = TCPS_ESTABLISHED;
				if (src->state == TCPS_ESTABLISHED &&
				    (*state)->src_node != NULL &&
				    pf_src_connlimit(state)) {
					REASON_SET(reason, PFRES_SRCLIMIT);
					return PF_DROP;
				}
			} else if (dst->state == TCPS_CLOSING) {
				dst->state = TCPS_FIN_WAIT_2;
			}
		}
		if (th->th_flags & TH_RST) {
			src->state = dst->state = TCPS_TIME_WAIT;
		}

		/* update expire time */
		(*state)->expire = pf_time_second();
		if (src->state >= TCPS_FIN_WAIT_2 &&
		    dst->state >= TCPS_FIN_WAIT_2) {
			(*state)->timeout = PFTM_TCP_CLOSED;
		} else if (src->state >= TCPS_CLOSING &&
		    dst->state >= TCPS_CLOSING) {
			(*state)->timeout = PFTM_TCP_FIN_WAIT;
		} else if (src->state < TCPS_ESTABLISHED ||
		    dst->state < TCPS_ESTABLISHED) {
			(*state)->timeout = PFTM_TCP_OPENING;
		} else if (src->state >= TCPS_CLOSING ||
		    dst->state >= TCPS_CLOSING) {
			(*state)->timeout = PFTM_TCP_CLOSING;
		} else {
			(*state)->timeout = PFTM_TCP_ESTABLISHED;
		}

		/* Fall through to PASS packet */
	} else if ((dst->state < TCPS_SYN_SENT ||
	    dst->state >= TCPS_FIN_WAIT_2 || src->state >= TCPS_FIN_WAIT_2) &&
	    SEQ_GEQ(src->seqhi + MAXACKWINDOW, end) &&
	    /* Within a window forward of the originating packet */
	    SEQ_GEQ(seq, src->seqlo - MAXACKWINDOW)) {
		/* Within a window backward of the originating packet */

		/*
		 * This currently handles three situations:
		 *  1) Stupid stacks will shotgun SYNs before their peer
		 *     replies.
		 *  2) When PF catches an already established stream (the
		 *     firewall rebooted, the state table was flushed, routes
		 *     changed...)
		 *  3) Packets get funky immediately after the connection
		 *     closes (this should catch Solaris spurious ACK|FINs
		 *     that web servers like to spew after a close)
		 *
		 * This must be a little more careful than the above code
		 * since packet floods will also be caught here. We don't
		 * update the TTL here to mitigate the damage of a packet
		 * flood and so the same code can handle awkward establishment
		 * and a loosened connection close.
		 * In the establishment case, a correct peer response will
		 * validate the connection, go through the normal state code
		 * and keep updating the state TTL.
		 */

		if (pf_status.debug >= PF_DEBUG_MISC) {
			printf("pf: loose state match: ");
			pf_print_state(*state);
			pf_print_flags(th->th_flags);
			printf(" seq=%u (%u) ack=%u len=%u ackskew=%d "
			    "pkts=%llu:%llu dir=%s,%s\n", seq, orig_seq, ack,
			    pd->p_len, ackskew, (*state)->packets[0],
			    (*state)->packets[1],
			    direction == PF_IN ? "in" : "out",
			    direction == sk->direction ?
			    "fwd" : "rev");
		}

		if (dst->scrub || src->scrub) {
			if (pf_normalize_tcp_stateful(pbuf, off, pd, reason, th,
			    *state, src, dst, &copyback)) {
				return PF_DROP;
			}
			pbuf = pd->mp;  // XXXSCW: Why?
		}

		/* update max window */
		if (src->max_win < win) {
			src->max_win = win;
		}
		/* synchronize sequencing */
		if (SEQ_GT(end, src->seqlo)) {
			src->seqlo = end;
		}
		/* slide the window of what the other end can send */
		if (SEQ_GEQ(ack + ((u_int32_t)win << sws), dst->seqhi)) {
			dst->seqhi = ack + MAX(((u_int32_t)win << sws), 1);
		}

		/*
		 * Cannot set dst->seqhi here since this could be a shotgunned
		 * SYN and not an already established connection.
		 */

		if (th->th_flags & TH_FIN) {
			if (src->state < TCPS_CLOSING) {
				src->state = TCPS_CLOSING;
			}
		}
		if (th->th_flags & TH_RST) {
			src->state = dst->state = TCPS_TIME_WAIT;
		}

		/* Fall through to PASS packet */
	} else {
		if (dst->state == TCPS_SYN_SENT &&
		    src->state == TCPS_SYN_SENT) {
			/* Send RST for state mismatches during handshake */
			if (!(th->th_flags & TH_RST)) {
				pf_send_tcp((*state)->rule.ptr, pd->af,
				    pd->dst, pd->src, th->th_dport,
				    th->th_sport, ntohl(th->th_ack), 0,
				    TH_RST, 0, 0,
				    (*state)->rule.ptr->return_ttl, 1, 0,
				    pd->eh, kif->pfik_ifp);
			}
			src->seqlo = 0;
			src->seqhi = 1;
			src->max_win = 1;
		} else if (pf_status.debug >= PF_DEBUG_MISC) {
			printf("pf: BAD state: ");
			pf_print_state(*state);
			pf_print_flags(th->th_flags);
			printf("\n   seq=%u (%u) ack=%u len=%u ackskew=%d "
			    "sws=%u dws=%u pkts=%llu:%llu dir=%s,%s\n",
			    seq, orig_seq, ack, pd->p_len, ackskew,
			    (unsigned int)sws, (unsigned int)dws,
			    (*state)->packets[0], (*state)->packets[1],
			    direction == PF_IN ? "in" : "out",
			    direction == sk->direction ?
			    "fwd" : "rev");
			printf("pf: State failure on: %c %c %c %c | %c %c\n",
			    SEQ_GEQ(src->seqhi, end) ? ' ' : '1',
			    SEQ_GEQ(seq,
			    src->seqlo - ((u_int32_t)dst->max_win << dws)) ?
			    ' ': '2',
			    (ackskew >= -MAXACKWINDOW) ? ' ' : '3',
			    (ackskew <= (MAXACKWINDOW << sws)) ? ' ' : '4',
			    SEQ_GEQ(src->seqhi + MAXACKWINDOW, end) ?' ' :'5',
			    SEQ_GEQ(seq, src->seqlo - MAXACKWINDOW) ?' ' :'6');
		}
		REASON_SET(reason, PFRES_BADSTATE);
		return PF_DROP;
	}

	/* Any packets which have gotten here are to be passed */

	if (sk->app_state &&
	    sk->app_state->handler) {
		sk->app_state->handler(*state, direction,
		    off + (th->th_off << 2), pd, kif);
		if (pd->lmw < 0) {
			REASON_SET(reason, PFRES_MEMORY);
			return PF_DROP;
		}
		pbuf = pd->mp;  // XXXSCW: Why?
	}

	/* translate source/destination address, if necessary */
	if (STATE_TRANSLATE(sk)) {
		pd->naf = (pd->af == sk->af_lan) ? sk->af_gwy : sk->af_lan;

		if (direction == PF_OUT) {
			pf_change_ap(direction, pd->mp, pd->src, &th->th_sport,
			    pd->ip_sum, &th->th_sum, &sk->gwy.addr,
			    sk->gwy.xport.port, 0, pd->af, pd->naf, 1);
		} else {
			if (pd->af != pd->naf) {
				if (pd->af == sk->af_gwy) {
					pf_change_ap(direction, pd->mp, pd->dst,
					    &th->th_dport, pd->ip_sum,
					    &th->th_sum, &sk->lan.addr,
					    sk->lan.xport.port, 0,
					    pd->af, pd->naf, 0);

					pf_change_ap(direction, pd->mp, pd->src,
					    &th->th_sport, pd->ip_sum,
					    &th->th_sum, &sk->ext_lan.addr,
					    th->th_sport, 0, pd->af,
					    pd->naf, 0);
				} else {
					pf_change_ap(direction, pd->mp, pd->dst,
					    &th->th_dport, pd->ip_sum,
					    &th->th_sum, &sk->ext_gwy.addr,
					    th->th_dport, 0, pd->af,
					    pd->naf, 0);

					pf_change_ap(direction, pd->mp, pd->src,
					    &th->th_sport, pd->ip_sum,
					    &th->th_sum, &sk->gwy.addr,
					    sk->gwy.xport.port, 0, pd->af,
					    pd->naf, 0);
				}
			} else {
				pf_change_ap(direction, pd->mp, pd->dst,
				    &th->th_dport, pd->ip_sum,
				    &th->th_sum, &sk->lan.addr,
				    sk->lan.xport.port, 0, pd->af,
				    pd->naf, 1);
			}
		}

		copyback = off + sizeof(*th);
	}

	if (copyback) {
		if (pf_lazy_makewritable(pd, pbuf, copyback) == NULL) {
			REASON_SET(reason, PFRES_MEMORY);
			return PF_DROP;
		}

		/* Copyback sequence modulation or stateful scrub changes */
		pbuf_copy_back(pbuf, off, sizeof(*th), th, sizeof(*th));

		if (sk->af_lan != sk->af_gwy) {
			return pf_do_nat64(sk, pd, pbuf, off);
		}
	}
	return PF_PASS;
}

static __attribute__((noinline)) int
pf_test_state_udp(struct pf_state **state, int direction, struct pfi_kif *kif,
    pbuf_t *pbuf, int off, void *h, struct pf_pdesc *pd, u_short *reason)
{
#pragma unused(h)
	struct pf_state_peer    *__single src, *__single dst;
	struct pf_state_key_cmp  key;
	struct pf_state_key     *__single sk;
	struct udphdr           *__single uh = pf_pd_get_hdr_udp(pd);
	struct pf_app_state as;
	int action, extfilter;
	key.app_state = 0;
	key.proto_variant = PF_EXTFILTER_APD;

	key.proto = IPPROTO_UDP;
	key.af_lan = key.af_gwy = pd->af;

	/*
	 * For NAT64 the first time rule search and state creation
	 * is done on the incoming side only.
	 * Once the state gets created, NAT64's LAN side (ipv6) will
	 * not be able to find the state in ext-gwy tree as that normally
	 * is intended to be looked up for incoming traffic from the
	 * WAN side.
	 * Therefore to handle NAT64 case we init keys here for both
	 * lan-ext as well as ext-gwy trees.
	 * In the state lookup we attempt a lookup on both trees if
	 * first one does not return any result and return a match if
	 * the match state's was created by NAT64 rule.
	 */
	PF_ACPY(&key.ext_gwy.addr, pd->src, key.af_gwy);
	PF_ACPY(&key.gwy.addr, pd->dst, key.af_gwy);
	key.ext_gwy.xport.port = uh->uh_sport;
	key.gwy.xport.port = uh->uh_dport;

	PF_ACPY(&key.lan.addr, pd->src, key.af_lan);
	PF_ACPY(&key.ext_lan.addr, pd->dst, key.af_lan);
	key.lan.xport.port = uh->uh_sport;
	key.ext_lan.xport.port = uh->uh_dport;

	if (ntohs(uh->uh_sport) == PF_IKE_PORT &&
	    ntohs(uh->uh_dport) == PF_IKE_PORT) {
		struct pf_ike_hdr ike;
		size_t plen = pbuf->pb_packet_len - off - sizeof(*uh);
		if (plen < PF_IKE_PACKET_MINSIZE) {
			DPFPRINTF(PF_DEBUG_MISC,
			    ("pf: IKE message too small.\n"));
			return PF_DROP;
		}

		if (plen > sizeof(ike)) {
			plen = sizeof(ike);
		}
		pbuf_copy_data(pbuf, off + sizeof(*uh), plen, &ike, sizeof(ike));

		if (ike.initiator_cookie) {
			key.app_state = &as;
			as.compare_lan_ext = pf_ike_compare;
			as.compare_ext_gwy = pf_ike_compare;
			as.u.ike.cookie = ike.initiator_cookie;
		} else {
			/*
			 * <http://tools.ietf.org/html/\
			 *    draft-ietf-ipsec-nat-t-ike-01>
			 * Support non-standard NAT-T implementations that
			 * push the ESP packet over the top of the IKE packet.
			 * Do not drop packet.
			 */
			DPFPRINTF(PF_DEBUG_MISC,
			    ("pf: IKE initiator cookie = 0.\n"));
		}
	}

	*state = pf_find_state(kif, &key, direction);

	if (!key.app_state && *state == 0) {
		key.proto_variant = PF_EXTFILTER_AD;
		*state = pf_find_state(kif, &key, direction);
	}

	if (!key.app_state && *state == 0) {
		key.proto_variant = PF_EXTFILTER_EI;
		*state = pf_find_state(kif, &key, direction);
	}

	/* similar to STATE_LOOKUP() */
	if (*state != NULL && pd != NULL && !(pd->pktflags & PKTF_FLOW_ID)) {
		pd->flowsrc = (*state)->state_key->flowsrc;
		pd->flowhash = (*state)->state_key->flowhash;
		if (pd->flowhash != 0) {
			pd->pktflags |= PKTF_FLOW_ID;
			pd->pktflags &= ~PKTF_FLOW_ADV;
		}
	}

	if (pf_state_lookup_aux(state, kif, direction, &action)) {
		return action;
	}

	sk = (*state)->state_key;

	/*
	 * In case of NAT64 the translation is first applied on the LAN
	 * side. Therefore for stack's address family comparison
	 * we use sk->af_lan.
	 */
	if ((direction == sk->direction) && (pd->af == sk->af_lan)) {
		src = &(*state)->src;
		dst = &(*state)->dst;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

	/* update states */
	if (src->state < PFUDPS_SINGLE) {
		src->state = PFUDPS_SINGLE;
	}
	if (dst->state == PFUDPS_SINGLE) {
		dst->state = PFUDPS_MULTIPLE;
	}

	/* update expire time */
	(*state)->expire = pf_time_second();
	if (src->state == PFUDPS_MULTIPLE && dst->state == PFUDPS_MULTIPLE) {
		(*state)->timeout = PFTM_UDP_MULTIPLE;
	} else {
		(*state)->timeout = PFTM_UDP_SINGLE;
	}

	extfilter = sk->proto_variant;
	if (extfilter > PF_EXTFILTER_APD) {
		if (direction == PF_OUT) {
			sk->ext_lan.xport.port = key.ext_lan.xport.port;
			if (extfilter > PF_EXTFILTER_AD) {
				PF_ACPY(&sk->ext_lan.addr, &key.ext_lan.addr,
				    key.af_lan);
			}
		} else {
			sk->ext_gwy.xport.port = key.ext_gwy.xport.port;
			if (extfilter > PF_EXTFILTER_AD) {
				PF_ACPY(&sk->ext_gwy.addr, &key.ext_gwy.addr,
				    key.af_gwy);
			}
		}
	}

	if (sk->app_state && sk->app_state->handler) {
		sk->app_state->handler(*state, direction, off + uh->uh_ulen,
		    pd, kif);
		if (pd->lmw < 0) {
			REASON_SET(reason, PFRES_MEMORY);
			return PF_DROP;
		}
		pbuf = pd->mp;  // XXXSCW: Why?
	}

	/* translate source/destination address, if necessary */
	if (STATE_TRANSLATE(sk)) {
		if (pf_lazy_makewritable(pd, pbuf, off + sizeof(*uh)) == NULL) {
			REASON_SET(reason, PFRES_MEMORY);
			return PF_DROP;
		}

		pd->naf = (pd->af == sk->af_lan) ? sk->af_gwy : sk->af_lan;

		if (direction == PF_OUT) {
			pf_change_ap(direction, pd->mp, pd->src, &uh->uh_sport,
			    pd->ip_sum, &uh->uh_sum, &sk->gwy.addr,
			    sk->gwy.xport.port, 1, pd->af, pd->naf, 1);
		} else {
			if (pd->af != pd->naf) {
				if (pd->af == sk->af_gwy) {
					pf_change_ap(direction, pd->mp, pd->dst,
					    &uh->uh_dport, pd->ip_sum,
					    &uh->uh_sum, &sk->lan.addr,
					    sk->lan.xport.port, 1,
					    pd->af, pd->naf, 0);

					pf_change_ap(direction, pd->mp, pd->src,
					    &uh->uh_sport, pd->ip_sum,
					    &uh->uh_sum, &sk->ext_lan.addr,
					    uh->uh_sport, 1, pd->af,
					    pd->naf, 0);
				} else {
					pf_change_ap(direction, pd->mp, pd->dst,
					    &uh->uh_dport, pd->ip_sum,
					    &uh->uh_sum, &sk->ext_gwy.addr,
					    uh->uh_dport, 1, pd->af,
					    pd->naf, 0);

					pf_change_ap(direction, pd->mp, pd->src,
					    &uh->uh_sport, pd->ip_sum,
					    &uh->uh_sum, &sk->gwy.addr,
					    sk->gwy.xport.port, 1, pd->af,
					    pd->naf, 0);
				}
			} else {
				pf_change_ap(direction, pd->mp, pd->dst,
				    &uh->uh_dport, pd->ip_sum,
				    &uh->uh_sum, &sk->lan.addr,
				    sk->lan.xport.port, 1,
				    pd->af, pd->naf, 1);
			}
		}

		pbuf_copy_back(pbuf, off, sizeof(*uh), uh, sizeof(*uh));
		if (sk->af_lan != sk->af_gwy) {
			return pf_do_nat64(sk, pd, pbuf, off);
		}
	}
	return PF_PASS;
}

static u_int32_t
pf_compute_packet_icmp_gencnt(uint32_t af, u_int32_t type, u_int32_t code)
{
	if (af == PF_INET) {
		if (type != ICMP_UNREACH && type != ICMP_TIMXCEED) {
			return 0;
		}
	} else {
		if (type != ICMP6_DST_UNREACH && type != ICMP6_PARAM_PROB &&
		    type != ICMP6_TIME_EXCEEDED) {
			return 0;
		}
	}
	return (af << 24) | (type << 16) | (code << 8);
}


static __attribute__((noinline)) int
pf_test_state_icmp(struct pf_state **state, int direction, struct pfi_kif *kif,
    pbuf_t *pbuf, int off, void *h, struct pf_pdesc *pd, u_short *reason)
{
#pragma unused(h)
	struct pf_addr  *__single saddr = pd->src, *__single daddr = pd->dst;
	struct in_addr  srcv4_inaddr = saddr->v4addr;
	u_int16_t        icmpid = 0, *__single icmpsum = NULL;
	u_int8_t         icmptype = 0;
	u_int32_t        icmpcode = 0;
	int              state_icmp = 0;
	struct pf_state_key_cmp key;
	struct pf_state_key     *__single sk;

	struct pf_app_state as;
	key.app_state = 0;

	pd->off = off;

	switch (pd->proto) {
#if INET
	case IPPROTO_ICMP:
		icmptype = pf_pd_get_hdr_icmp(pd)->icmp_type;
		icmpid = pf_pd_get_hdr_icmp(pd)->icmp_id;
		icmpsum = &pf_pd_get_hdr_icmp(pd)->icmp_cksum;
		icmpcode = pf_pd_get_hdr_icmp(pd)->icmp_code;

		if (ICMP_ERRORTYPE(icmptype)) {
			state_icmp++;
		}
		break;
#endif /* INET */
	case IPPROTO_ICMPV6:
		icmptype = pf_pd_get_hdr_icmp6(pd)->icmp6_type;
		icmpid = pf_pd_get_hdr_icmp6(pd)->icmp6_id;
		icmpsum = &pf_pd_get_hdr_icmp6(pd)->icmp6_cksum;
		icmpcode = pf_pd_get_hdr_icmp6(pd)->icmp6_code;

		if (ICMP6_ERRORTYPE(icmptype)) {
			state_icmp++;
		}
		break;
	}

	if (pbuf != NULL && pbuf->pb_flow_gencnt != NULL &&
	    *pbuf->pb_flow_gencnt == 0) {
		u_int32_t af = pd->proto == IPPROTO_ICMP ? PF_INET : PF_INET6;
		*pbuf->pb_flow_gencnt = pf_compute_packet_icmp_gencnt(af, icmptype, icmpcode);
	}

	if (!state_icmp) {
		/*
		 * ICMP query/reply message not related to a TCP/UDP packet.
		 * Search for an ICMP state.
		 */
		/*
		 * NAT64 requires protocol translation  between ICMPv4
		 * and ICMPv6. TCP and UDP do not require protocol
		 * translation. To avoid adding complexity just to
		 * handle ICMP(v4addr/v6addr), we always lookup  for
		 * proto = IPPROTO_ICMP on both LAN and WAN side
		 */
		key.proto = IPPROTO_ICMP;
		key.af_lan = key.af_gwy = pd->af;

		PF_ACPY(&key.ext_gwy.addr, pd->src, key.af_gwy);
		PF_ACPY(&key.gwy.addr, pd->dst, key.af_gwy);
		key.ext_gwy.xport.port = 0;
		key.gwy.xport.port = icmpid;

		PF_ACPY(&key.lan.addr, pd->src, key.af_lan);
		PF_ACPY(&key.ext_lan.addr, pd->dst, key.af_lan);
		key.lan.xport.port = icmpid;
		key.ext_lan.xport.port = 0;

		STATE_LOOKUP();

		sk = (*state)->state_key;
		(*state)->expire = pf_time_second();
		(*state)->timeout = PFTM_ICMP_ERROR_REPLY;

		/* translate source/destination address, if necessary */
		if (STATE_TRANSLATE(sk)) {
			pd->naf = (pd->af == sk->af_lan) ?
			    sk->af_gwy : sk->af_lan;
			if (direction == PF_OUT) {
				switch (pd->af) {
#if INET
				case AF_INET:
					pf_change_a(&saddr->v4addr.s_addr,
					    pd->ip_sum,
					    sk->gwy.addr.v4addr.s_addr, 0);
					pf_pd_get_hdr_icmp(pd)->icmp_cksum =
					    pf_cksum_fixup(
						pf_pd_get_hdr_icmp(pd)->icmp_cksum, icmpid,
						sk->gwy.xport.port, 0);
					pf_pd_get_hdr_icmp(pd)->icmp_id =
					    sk->gwy.xport.port;
					if (pf_lazy_makewritable(pd, pbuf,
					    off + ICMP_MINLEN) == NULL) {
						return PF_DROP;
					}
					pbuf_copy_back(pbuf, off, ICMP_MINLEN,
					    pf_pd_get_hdr_ptr_icmp(pd), sizeof(struct icmp));
					break;
#endif /* INET */
				case AF_INET6:
					pf_change_a6(saddr,
					    &pf_pd_get_hdr_icmp6(pd)->icmp6_cksum,
					    &sk->gwy.addr, 0);
					if (pf_lazy_makewritable(pd, pbuf,
					    off + sizeof(struct icmp6_hdr)) ==
					    NULL) {
						return PF_DROP;
					}
					pbuf_copy_back(pbuf, off,
					    sizeof(struct icmp6_hdr),
					    pf_pd_get_hdr_ptr_icmp6(pd), sizeof(struct icmp6_hdr));
					break;
				}
			} else {
				switch (pd->af) {
#if INET
				case AF_INET:
					if (pd->naf != AF_INET) {
						if (pf_translate_icmp_af(
							    AF_INET6, pf_pd_get_hdr_icmp(pd))) {
							return PF_DROP;
						}

						pd->proto = IPPROTO_ICMPV6;
					} else {
						pf_change_a(&daddr->v4addr.s_addr,
						    pd->ip_sum,
						    sk->lan.addr.v4addr.s_addr, 0);

						pf_pd_get_hdr_icmp(pd)->icmp_cksum =
						    pf_cksum_fixup(
							pf_pd_get_hdr_icmp(pd)->icmp_cksum,
							icmpid, sk->lan.xport.port, 0);

						pf_pd_get_hdr_icmp(pd)->icmp_id =
						    sk->lan.xport.port;
					}

					if (pf_lazy_makewritable(pd, pbuf,
					    off + ICMP_MINLEN) == NULL) {
						return PF_DROP;
					}
					pbuf_copy_back(pbuf, off, ICMP_MINLEN,
					    pf_pd_get_hdr_ptr_icmp(pd), sizeof(struct icmp));
					if (sk->af_lan != sk->af_gwy) {
						return pf_do_nat64(sk, pd,
						           pbuf, off);
					}
					break;
#endif /* INET */
				case AF_INET6:
					if (pd->naf != AF_INET6) {
						if (pf_translate_icmp_af(
							    AF_INET, pf_pd_get_hdr_icmp6(pd))) {
							return PF_DROP;
						}

						pd->proto = IPPROTO_ICMP;
					} else {
						pf_change_a6(daddr,
						    &pf_pd_get_hdr_icmp6(pd)->icmp6_cksum,
						    &sk->lan.addr, 0);
					}
					if (pf_lazy_makewritable(pd, pbuf,
					    off + sizeof(struct icmp6_hdr)) ==
					    NULL) {
						return PF_DROP;
					}
					pbuf_copy_back(pbuf, off,
					    sizeof(struct icmp6_hdr),
					    pf_pd_get_hdr_ptr_icmp6(pd), sizeof(struct icmp6_hdr));
					if (sk->af_lan != sk->af_gwy) {
						return pf_do_nat64(sk, pd,
						           pbuf, off);
					}
					break;
				}
			}
		}

		return PF_PASS;
	} else {
		/*
		 * ICMP error message in response to a TCP/UDP packet.
		 * Extract the inner TCP/UDP header and search for that state.
		 */
		struct pf_pdesc pd2; /* For inner (original) header */
#if INET
		struct ip       h2;
#endif /* INET */
		struct ip6_hdr  h2_6;
		int             terminal = 0;
		int             ipoff2 = 0;
		int             off2 = 0;

		memset(&pd2, 0, sizeof(pd2));

		pd2.af = pd->af;
		switch (pd->af) {
#if INET
		case AF_INET:
			/* offset of h2 in mbuf chain */
			ipoff2 = off + ICMP_MINLEN;

			if (!pf_pull_hdr(pbuf, ipoff2, &h2, sizeof(h2), sizeof(h2),
			    NULL, reason, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short "
				    "(ip)\n"));
				return PF_DROP;
			}
			/*
			 * ICMP error messages don't refer to non-first
			 * fragments
			 */
			if (h2.ip_off & htons(IP_OFFMASK)) {
				REASON_SET(reason, PFRES_FRAG);
				return PF_DROP;
			}

			/* offset of protocol header that follows h2 */
			off2 = ipoff2 + (h2.ip_hl << 2);
			/* TODO */
			pd2.off = ipoff2 + (h2.ip_hl << 2);

			pd2.proto = h2.ip_p;
			pd2.src = (struct pf_addr *)&h2.ip_src;
			pd2.dst = (struct pf_addr *)&h2.ip_dst;
			pd2.ip_sum = &h2.ip_sum;
			break;
#endif /* INET */
		case AF_INET6:
			ipoff2 = off + sizeof(struct icmp6_hdr);

			if (!pf_pull_hdr(pbuf, ipoff2, &h2_6, sizeof(h2_6), sizeof(h2_6),
			    NULL, reason, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short "
				    "(ip6)\n"));
				return PF_DROP;
			}
			pd2.proto = h2_6.ip6_nxt;
			pd2.src = (struct pf_addr *)(void *)&h2_6.ip6_src;
			pd2.dst = (struct pf_addr *)(void *)&h2_6.ip6_dst;
			pd2.ip_sum = NULL;
			off2 = ipoff2 + sizeof(h2_6);
			do {
				switch (pd2.proto) {
				case IPPROTO_FRAGMENT:
					/*
					 * ICMPv6 error messages for
					 * non-first fragments
					 */
					REASON_SET(reason, PFRES_FRAG);
					return PF_DROP;
				case IPPROTO_AH:
				case IPPROTO_HOPOPTS:
				case IPPROTO_ROUTING:
				case IPPROTO_DSTOPTS: {
					/* get next header and header length */
					struct ip6_ext opt6;

					if (!pf_pull_hdr(pbuf, off2, &opt6, sizeof(opt6),
					    sizeof(opt6), NULL, reason,
					    pd2.af)) {
						DPFPRINTF(PF_DEBUG_MISC,
						    ("pf: ICMPv6 short opt\n"));
						return PF_DROP;
					}
					if (pd2.proto == IPPROTO_AH) {
						off2 += (opt6.ip6e_len + 2) * 4;
					} else {
						off2 += (opt6.ip6e_len + 1) * 8;
					}
					pd2.proto = opt6.ip6e_nxt;
					/* goto the next header */
					break;
				}
				default:
					terminal++;
					break;
				}
			} while (!terminal);
			/* TODO */
			pd2.off = ipoff2;
			break;
		}

		switch (pd2.proto) {
		case IPPROTO_TCP: {
			struct tcphdr            th;
			u_int32_t                seq;
			struct pf_state_peer    *src, *dst;
			u_int8_t                 dws;
			int                      copyback = 0;

			/*
			 * Only the first 8 bytes of the TCP header can be
			 * expected. Don't access any TCP header fields after
			 * th_seq, an ackskew test is not possible.
			 */
			if (!pf_pull_hdr(pbuf, off2, &th, sizeof(th), 8, NULL, reason,
			    pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short "
				    "(tcp)\n"));
				return PF_DROP;
			}

			key.proto = IPPROTO_TCP;
			key.af_gwy = pd2.af;
			PF_ACPY(&key.ext_gwy.addr, pd2.dst, key.af_gwy);
			PF_ACPY(&key.gwy.addr, pd2.src, key.af_gwy);
			key.ext_gwy.xport.port = th.th_dport;
			key.gwy.xport.port = th.th_sport;

			key.af_lan = pd2.af;
			PF_ACPY(&key.lan.addr, pd2.dst, key.af_lan);
			PF_ACPY(&key.ext_lan.addr, pd2.src, key.af_lan);
			key.lan.xport.port = th.th_dport;
			key.ext_lan.xport.port = th.th_sport;

			STATE_LOOKUP();

			sk = (*state)->state_key;
			if ((direction == sk->direction) &&
			    ((sk->af_lan == sk->af_gwy) ||
			    (pd2.af == sk->af_lan))) {
				src = &(*state)->dst;
				dst = &(*state)->src;
			} else {
				src = &(*state)->src;
				dst = &(*state)->dst;
			}

			if (src->wscale && (dst->wscale & PF_WSCALE_FLAG)) {
				dws = dst->wscale & PF_WSCALE_MASK;
			} else {
				dws = TCP_MAX_WINSHIFT;
			}

			/* Demodulate sequence number */
			seq = ntohl(th.th_seq) - src->seqdiff;
			if (src->seqdiff) {
				pf_change_a(&th.th_seq, icmpsum,
				    htonl(seq), 0);
				copyback = 1;
			}

			if (!SEQ_GEQ(src->seqhi, seq) ||
			    !SEQ_GEQ(seq,
			    src->seqlo - ((u_int32_t)dst->max_win << dws))) {
				if (pf_status.debug >= PF_DEBUG_MISC) {
					printf("pf: BAD ICMP %d:%d ",
					    icmptype, pf_pd_get_hdr_icmp(pd)->icmp_code);
					pf_print_host(pd->src, 0, pd->af);
					printf(" -> ");
					pf_print_host(pd->dst, 0, pd->af);
					printf(" state: ");
					pf_print_state(*state);
					printf(" seq=%u\n", seq);
				}
				REASON_SET(reason, PFRES_BADSTATE);
				return PF_DROP;
			}

			pd->naf = pd2.naf = (pd2.af == sk->af_lan) ?
			    sk->af_gwy : sk->af_lan;

			if (STATE_TRANSLATE(sk)) {
				/* NAT64 case */
				if (sk->af_lan != sk->af_gwy) {
					struct pf_state_host *saddr2, *daddr2;

					if (pd2.naf == sk->af_lan) {
						saddr2 = &sk->lan;
						daddr2 = &sk->ext_lan;
					} else {
						saddr2 = &sk->ext_gwy;
						daddr2 = &sk->gwy;
					}

					/* translate ICMP message types and codes */
					if (pf_translate_icmp_af(pd->naf,
					    pf_pd_get_hdr_icmp(pd))) {
						return PF_DROP;
					}

					if (pf_lazy_makewritable(pd, pbuf,
					    off2 + 8) == NULL) {
						return PF_DROP;
					}

					pbuf_copy_back(pbuf, pd->off,
					    sizeof(struct icmp6_hdr),
					    pf_pd_get_hdr_ptr_icmp6(pd), pd->hdrmaxlen);

					/*
					 * translate inner ip header within the
					 * ICMP message
					 */
					if (pf_change_icmp_af(pbuf, ipoff2, pd,
					    &pd2, &saddr2->addr, &daddr2->addr,
					    pd->af, pd->naf)) {
						return PF_DROP;
					}

					if (pd->naf == AF_INET) {
						pd->proto = IPPROTO_ICMP;
					} else {
						pd->proto = IPPROTO_ICMPV6;
					}

					/*
					 * translate inner tcp header within
					 * the ICMP message
					 */
					pf_change_ap(direction, NULL, pd2.src,
					    &th.th_sport, pd2.ip_sum,
					    &th.th_sum, &daddr2->addr,
					    saddr2->xport.port, 0, pd2.af,
					    pd2.naf, 0);

					pf_change_ap(direction, NULL, pd2.dst,
					    &th.th_dport, pd2.ip_sum,
					    &th.th_sum, &saddr2->addr,
					    daddr2->xport.port, 0, pd2.af,
					    pd2.naf, 0);

					pbuf_copy_back(pbuf, pd2.off, 8, &th, sizeof(th));

					/* translate outer ip header */
					PF_ACPY(&pd->naddr, &daddr2->addr,
					    pd->naf);
					PF_ACPY(&pd->ndaddr, &saddr2->addr,
					    pd->naf);
					if (pd->af == AF_INET) {
						memcpy(&pd->naddr.addr32[3],
						    &srcv4_inaddr,
						    sizeof(pd->naddr.addr32[3]));
						return pf_nat64_ipv4(pbuf, off,
						           pd);
					} else {
						return pf_nat64_ipv6(pbuf, off,
						           pd);
					}
				}
				if (direction == PF_IN) {
					pf_change_icmp(pd2.src, &th.th_sport,
					    daddr, &sk->lan.addr,
					    sk->lan.xport.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, pd2.af);
				} else {
					pf_change_icmp(pd2.dst, &th.th_dport,
					    saddr, &sk->gwy.addr,
					    sk->gwy.xport.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, pd2.af);
				}
				copyback = 1;
			}

			if (copyback) {
				if (pf_lazy_makewritable(pd, pbuf, off2 + 8) ==
				    NULL) {
					return PF_DROP;
				}
				switch (pd2.af) {
#if INET
				case AF_INET:
					pbuf_copy_back(pbuf, off, ICMP_MINLEN,
					    pf_pd_get_hdr_ptr_icmp(pd), pd->hdrmaxlen);
					pbuf_copy_back(pbuf, ipoff2, sizeof(h2),
					    &h2, sizeof(h2));
					break;
#endif /* INET */
				case AF_INET6:
					pbuf_copy_back(pbuf, off,
					    sizeof(struct icmp6_hdr),
					    pf_pd_get_hdr_ptr_icmp6(pd), pd->hdrmaxlen);
					pbuf_copy_back(pbuf, ipoff2,
					    sizeof(h2_6), &h2_6, sizeof(h2_6));
					break;
				}
				pbuf_copy_back(pbuf, off2, 8, &th, sizeof(th));
			}

			return PF_PASS;
		}
		case IPPROTO_UDP: {
			struct udphdr uh;
			int dx, action;
			if (!pf_pull_hdr(pbuf, off2, &uh, sizeof(uh), sizeof(uh),
			    NULL, reason, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short "
				    "(udp)\n"));
				return PF_DROP;
			}

			key.af_gwy = pd2.af;
			PF_ACPY(&key.ext_gwy.addr, pd2.dst, key.af_gwy);
			PF_ACPY(&key.gwy.addr, pd2.src, key.af_gwy);
			key.ext_gwy.xport.port = uh.uh_dport;
			key.gwy.xport.port = uh.uh_sport;

			key.af_lan = pd2.af;
			PF_ACPY(&key.lan.addr, pd2.dst, key.af_lan);
			PF_ACPY(&key.ext_lan.addr, pd2.src, key.af_lan);
			key.lan.xport.port = uh.uh_dport;
			key.ext_lan.xport.port = uh.uh_sport;

			key.proto = IPPROTO_UDP;
			key.proto_variant = PF_EXTFILTER_APD;
			dx = direction;

			if (ntohs(uh.uh_sport) == PF_IKE_PORT &&
			    ntohs(uh.uh_dport) == PF_IKE_PORT) {
				struct pf_ike_hdr ike;
				size_t plen = pbuf->pb_packet_len - off2 -
				    sizeof(uh);
				if (direction == PF_IN &&
				    plen < 8 /* PF_IKE_PACKET_MINSIZE */) {
					DPFPRINTF(PF_DEBUG_MISC, ("pf: "
					    "ICMP error, embedded IKE message "
					    "too small.\n"));
					return PF_DROP;
				}

				if (plen > sizeof(ike)) {
					plen = sizeof(ike);
				}
				pbuf_copy_data(pbuf, off + sizeof(uh), plen,
				    &ike, sizeof(ike));

				key.app_state = &as;
				as.compare_lan_ext = pf_ike_compare;
				as.compare_ext_gwy = pf_ike_compare;
				as.u.ike.cookie = ike.initiator_cookie;
			}

			*state = pf_find_state(kif, &key, dx);

			if (key.app_state && *state == 0) {
				key.app_state = 0;
				*state = pf_find_state(kif, &key, dx);
			}

			if (*state == 0) {
				key.proto_variant = PF_EXTFILTER_AD;
				*state = pf_find_state(kif, &key, dx);
			}

			if (*state == 0) {
				key.proto_variant = PF_EXTFILTER_EI;
				*state = pf_find_state(kif, &key, dx);
			}

			/* similar to STATE_LOOKUP() */
			if (*state != NULL && pd != NULL &&
			    !(pd->pktflags & PKTF_FLOW_ID)) {
				pd->flowsrc = (*state)->state_key->flowsrc;
				pd->flowhash = (*state)->state_key->flowhash;
				if (pd->flowhash != 0) {
					pd->pktflags |= PKTF_FLOW_ID;
					pd->pktflags &= ~PKTF_FLOW_ADV;
				}
			}

			if (pf_state_lookup_aux(state, kif, direction, &action)) {
				return action;
			}

			sk = (*state)->state_key;
			pd->naf = pd2.naf = (pd2.af == sk->af_lan) ?
			    sk->af_gwy : sk->af_lan;

			if (STATE_TRANSLATE(sk)) {
				/* NAT64 case */
				if (sk->af_lan != sk->af_gwy) {
					struct pf_state_host *saddr2, *daddr2;

					if (pd2.naf == sk->af_lan) {
						saddr2 = &sk->lan;
						daddr2 = &sk->ext_lan;
					} else {
						saddr2 = &sk->ext_gwy;
						daddr2 = &sk->gwy;
					}

					/* translate ICMP message */
					if (pf_translate_icmp_af(pd->naf,
					    pf_pd_get_hdr_icmp(pd))) {
						return PF_DROP;
					}
					if (pf_lazy_makewritable(pd, pbuf,
					    off2 + 8) == NULL) {
						return PF_DROP;
					}

					pbuf_copy_back(pbuf, pd->off,
					    sizeof(struct icmp6_hdr),
					    pf_pd_get_hdr_ptr_icmp6(pd), pd->hdrmaxlen);

					/*
					 * translate inner ip header within the
					 * ICMP message
					 */
					if (pf_change_icmp_af(pbuf, ipoff2, pd,
					    &pd2, &saddr2->addr, &daddr2->addr,
					    pd->af, pd->naf)) {
						return PF_DROP;
					}

					if (pd->naf == AF_INET) {
						pd->proto = IPPROTO_ICMP;
					} else {
						pd->proto = IPPROTO_ICMPV6;
					}

					/*
					 * translate inner udp header within
					 * the ICMP message
					 */
					pf_change_ap(direction, NULL, pd2.src,
					    &uh.uh_sport, pd2.ip_sum,
					    &uh.uh_sum, &daddr2->addr,
					    saddr2->xport.port, 0, pd2.af,
					    pd2.naf, 0);

					pf_change_ap(direction, NULL, pd2.dst,
					    &uh.uh_dport, pd2.ip_sum,
					    &uh.uh_sum, &saddr2->addr,
					    daddr2->xport.port, 0, pd2.af,
					    pd2.naf, 0);

					pbuf_copy_back(pbuf, pd2.off,
					    sizeof(uh), &uh, sizeof(uh));

					/* translate outer ip header */
					PF_ACPY(&pd->naddr, &daddr2->addr,
					    pd->naf);
					PF_ACPY(&pd->ndaddr, &saddr2->addr,
					    pd->naf);
					if (pd->af == AF_INET) {
						memcpy(&pd->naddr.addr32[3],
						    &srcv4_inaddr,
						    sizeof(pd->naddr.addr32[3]));
						return pf_nat64_ipv4(pbuf, off,
						           pd);
					} else {
						return pf_nat64_ipv6(pbuf, off,
						           pd);
					}
				}
				if (direction == PF_IN) {
					pf_change_icmp(pd2.src, &uh.uh_sport,
					    daddr, &sk->lan.addr,
					    sk->lan.xport.port, &uh.uh_sum,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 1, pd2.af);
				} else {
					pf_change_icmp(pd2.dst, &uh.uh_dport,
					    saddr, &sk->gwy.addr,
					    sk->gwy.xport.port, &uh.uh_sum,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 1, pd2.af);
				}
				if (pf_lazy_makewritable(pd, pbuf,
				    off2 + sizeof(uh)) == NULL) {
					return PF_DROP;
				}
				switch (pd2.af) {
#if INET
				case AF_INET:
					pbuf_copy_back(pbuf, off, ICMP_MINLEN,
					    pf_pd_get_hdr_ptr_icmp(pd), pd->hdrmaxlen);
					pbuf_copy_back(pbuf, ipoff2,
					    sizeof(h2), &h2, sizeof(h2));
					break;
#endif /* INET */
				case AF_INET6:
					pbuf_copy_back(pbuf, off,
					    sizeof(struct icmp6_hdr),
					    pf_pd_get_hdr_ptr_icmp6(pd), pd->hdrmaxlen);
					pbuf_copy_back(pbuf, ipoff2,
					    sizeof(h2_6), &h2_6, sizeof(h2_6));
					break;
				}
				pbuf_copy_back(pbuf, off2, sizeof(uh), &uh, sizeof(uh));
			}

			return PF_PASS;
		}
#if INET
		case IPPROTO_ICMP: {
			struct icmp             iih;

			if (!pf_pull_hdr(pbuf, off2, &iih, sizeof(iih), ICMP_MINLEN,
			    NULL, reason, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short i"
				    "(icmp)\n"));
				return PF_DROP;
			}

			key.proto = IPPROTO_ICMP;
			if (direction == PF_IN) {
				key.af_gwy = pd2.af;
				PF_ACPY(&key.ext_gwy.addr, pd2.dst, key.af_gwy);
				PF_ACPY(&key.gwy.addr, pd2.src, key.af_gwy);
				key.ext_gwy.xport.port = 0;
				key.gwy.xport.port = iih.icmp_id;
			} else {
				key.af_lan = pd2.af;
				PF_ACPY(&key.lan.addr, pd2.dst, key.af_lan);
				PF_ACPY(&key.ext_lan.addr, pd2.src, key.af_lan);
				key.lan.xport.port = iih.icmp_id;
				key.ext_lan.xport.port = 0;
			}

			STATE_LOOKUP();

			sk = (*state)->state_key;
			if (STATE_TRANSLATE(sk)) {
				if (direction == PF_IN) {
					pf_change_icmp(pd2.src, &iih.icmp_id,
					    daddr, &sk->lan.addr,
					    sk->lan.xport.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, AF_INET);
				} else {
					pf_change_icmp(pd2.dst, &iih.icmp_id,
					    saddr, &sk->gwy.addr,
					    sk->gwy.xport.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, AF_INET);
				}
				if (pf_lazy_makewritable(pd, pbuf,
				    off2 + ICMP_MINLEN) == NULL) {
					return PF_DROP;
				}
				pbuf_copy_back(pbuf, off, ICMP_MINLEN,
				    pf_pd_get_hdr_ptr_icmp(pd), pd->hdrmaxlen);
				pbuf_copy_back(pbuf, ipoff2, sizeof(h2), &h2, sizeof(h2));
				pbuf_copy_back(pbuf, off2, ICMP_MINLEN, &iih, sizeof(iih));
			}

			return PF_PASS;
		}
#endif /* INET */
		case IPPROTO_ICMPV6: {
			struct icmp6_hdr        iih;

			if (!pf_pull_hdr(pbuf, off2, &iih, sizeof(iih),
			    sizeof(struct icmp6_hdr), NULL, reason, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short "
				    "(icmp6)\n"));
				return PF_DROP;
			}

			key.proto = IPPROTO_ICMPV6;
			if (direction == PF_IN) {
				key.af_gwy = pd2.af;
				PF_ACPY(&key.ext_gwy.addr, pd2.dst, key.af_gwy);
				PF_ACPY(&key.gwy.addr, pd2.src, key.af_gwy);
				key.ext_gwy.xport.port = 0;
				key.gwy.xport.port = iih.icmp6_id;
			} else {
				key.af_lan = pd2.af;
				PF_ACPY(&key.lan.addr, pd2.dst, key.af_lan);
				PF_ACPY(&key.ext_lan.addr, pd2.src, key.af_lan);
				key.lan.xport.port = iih.icmp6_id;
				key.ext_lan.xport.port = 0;
			}

			STATE_LOOKUP();

			sk = (*state)->state_key;
			if (STATE_TRANSLATE(sk)) {
				if (direction == PF_IN) {
					pf_change_icmp(pd2.src, &iih.icmp6_id,
					    daddr, &sk->lan.addr,
					    sk->lan.xport.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, AF_INET6);
				} else {
					pf_change_icmp(pd2.dst, &iih.icmp6_id,
					    saddr, &sk->gwy.addr,
					    sk->gwy.xport.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, AF_INET6);
				}
				if (pf_lazy_makewritable(pd, pbuf, off2 +
				    sizeof(struct icmp6_hdr)) == NULL) {
					return PF_DROP;
				}
				pbuf_copy_back(pbuf, off,
				    sizeof(struct icmp6_hdr),
				    pf_pd_get_hdr_ptr_icmp6(pd), pd->hdrmaxlen);
				pbuf_copy_back(pbuf, ipoff2, sizeof(h2_6),
				    &h2_6, sizeof(h2_6));
				pbuf_copy_back(pbuf, off2,
				    sizeof(struct icmp6_hdr), &iih, sizeof(iih));
			}

			return PF_PASS;
		}
		default: {
			key.proto = pd2.proto;
			if (direction == PF_IN) {
				key.af_gwy = pd2.af;
				PF_ACPY(&key.ext_gwy.addr, pd2.dst, key.af_gwy);
				PF_ACPY(&key.gwy.addr, pd2.src, key.af_gwy);
				key.ext_gwy.xport.port = 0;
				key.gwy.xport.port = 0;
			} else {
				key.af_lan = pd2.af;
				PF_ACPY(&key.lan.addr, pd2.dst, key.af_lan);
				PF_ACPY(&key.ext_lan.addr, pd2.src, key.af_lan);
				key.lan.xport.port = 0;
				key.ext_lan.xport.port = 0;
			}

			STATE_LOOKUP();

			sk = (*state)->state_key;
			if (STATE_TRANSLATE(sk)) {
				if (direction == PF_IN) {
					pf_change_icmp(pd2.src, NULL, daddr,
					    &sk->lan.addr, 0, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, pd2.af);
				} else {
					pf_change_icmp(pd2.dst, NULL, saddr,
					    &sk->gwy.addr, 0, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, pd2.af);
				}
				switch (pd2.af) {
#if INET
				case AF_INET:
					if (pf_lazy_makewritable(pd, pbuf,
					    ipoff2 + sizeof(h2)) == NULL) {
						return PF_DROP;
					}
					/*
					 * <XXXSCW>
					 * Xnu was missing the following...
					 */
					pbuf_copy_back(pbuf, off, ICMP_MINLEN,
					    pf_pd_get_hdr_ptr_icmp(pd), pd->hdrmaxlen);
					pbuf_copy_back(pbuf, ipoff2,
					    sizeof(h2), &h2, sizeof(h2));
					break;
					/*
					 * </XXXSCW>
					 */
#endif /* INET */
				case AF_INET6:
					if (pf_lazy_makewritable(pd, pbuf,
					    ipoff2 + sizeof(h2_6)) == NULL) {
						return PF_DROP;
					}
					pbuf_copy_back(pbuf, off,
					    sizeof(struct icmp6_hdr),
					    pf_pd_get_hdr_ptr_icmp6(pd), pd->hdrmaxlen);
					pbuf_copy_back(pbuf, ipoff2,
					    sizeof(h2_6), &h2_6, sizeof(h2_6));
					break;
				}
			}

			return PF_PASS;
		}
		}
	}
}

static __attribute__((noinline)) int
pf_test_state_grev1(struct pf_state **state, int direction,
    struct pfi_kif *kif, int off, struct pf_pdesc *pd)
{
	struct pf_state_peer *__single src;
	struct pf_state_peer *__single dst;
	struct pf_state_key_cmp key = {};
	struct pf_grev1_hdr *__single grev1 = pf_pd_get_hdr_grev1(pd);

	key.app_state = 0;
	key.proto = IPPROTO_GRE;
	key.proto_variant = PF_GRE_PPTP_VARIANT;
	if (direction == PF_IN) {
		key.af_gwy = pd->af;
		PF_ACPY(&key.ext_gwy.addr, pd->src, key.af_gwy);
		PF_ACPY(&key.gwy.addr, pd->dst, key.af_gwy);
		key.gwy.xport.call_id = grev1->call_id;
	} else {
		key.af_lan = pd->af;
		PF_ACPY(&key.lan.addr, pd->src, key.af_lan);
		PF_ACPY(&key.ext_lan.addr, pd->dst, key.af_lan);
		key.ext_lan.xport.call_id = grev1->call_id;
	}

	STATE_LOOKUP();

	if (direction == (*state)->state_key->direction) {
		src = &(*state)->src;
		dst = &(*state)->dst;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

	/* update states */
	if (src->state < PFGRE1S_INITIATING) {
		src->state = PFGRE1S_INITIATING;
	}

	/* update expire time */
	(*state)->expire = pf_time_second();
	if (src->state >= PFGRE1S_INITIATING &&
	    dst->state >= PFGRE1S_INITIATING) {
		if ((*state)->timeout != PFTM_TCP_ESTABLISHED) {
			(*state)->timeout = PFTM_GREv1_ESTABLISHED;
		}
		src->state = PFGRE1S_ESTABLISHED;
		dst->state = PFGRE1S_ESTABLISHED;
	} else {
		(*state)->timeout = PFTM_GREv1_INITIATING;
	}

	if ((*state)->state_key->app_state) {
		(*state)->state_key->app_state->u.grev1.pptp_state->expire =
		    pf_time_second();
	}

	/* translate source/destination address, if necessary */
	if (STATE_GRE_TRANSLATE((*state)->state_key)) {
		if (direction == PF_OUT) {
			switch (pd->af) {
#if INET
			case AF_INET:
				pf_change_a(&pd->src->v4addr.s_addr,
				    pd->ip_sum,
				    (*state)->state_key->gwy.addr.v4addr.s_addr, 0);
				break;
#endif /* INET */
			case AF_INET6:
				PF_ACPY(pd->src, &(*state)->state_key->gwy.addr,
				    pd->af);
				break;
			}
		} else {
			grev1->call_id = (*state)->state_key->lan.xport.call_id;

			switch (pd->af) {
#if INET
			case AF_INET:
				pf_change_a(&pd->dst->v4addr.s_addr,
				    pd->ip_sum,
				    (*state)->state_key->lan.addr.v4addr.s_addr, 0);
				break;
#endif /* INET */
			case AF_INET6:
				PF_ACPY(pd->dst, &(*state)->state_key->lan.addr,
				    pd->af);
				break;
			}
		}

		if (pf_lazy_makewritable(pd, pd->mp, off + sizeof(*grev1)) ==
		    NULL) {
			return PF_DROP;
		}
		pbuf_copy_back(pd->mp, off, sizeof(*grev1), grev1, sizeof(*grev1));
	}

	return PF_PASS;
}

static __attribute__((noinline)) int
pf_test_state_esp(struct pf_state **state, int direction, struct pfi_kif *kif,
    int off, struct pf_pdesc *pd)
{
#pragma unused(off)
	struct pf_state_peer *__single src;
	struct pf_state_peer *__single dst;
	struct pf_state_key_cmp key;
	struct pf_esp_hdr *__single esp = pf_pd_get_hdr_esp(pd);
	int action;

	memset(&key, 0, sizeof(key));
	key.proto = IPPROTO_ESP;
	if (direction == PF_IN) {
		key.af_gwy = pd->af;
		PF_ACPY(&key.ext_gwy.addr, pd->src, key.af_gwy);
		PF_ACPY(&key.gwy.addr, pd->dst, key.af_gwy);
		key.gwy.xport.spi = esp->spi;
	} else {
		key.af_lan = pd->af;
		PF_ACPY(&key.lan.addr, pd->src, key.af_lan);
		PF_ACPY(&key.ext_lan.addr, pd->dst, key.af_lan);
		key.ext_lan.xport.spi = esp->spi;
	}

	*state = pf_find_state(kif, &key, direction);

	if (*state == 0) {
		struct pf_state *s;

		/*
		 * <jhw@apple.com>
		 * No matching state.  Look for a blocking state.  If we find
		 * one, then use that state and move it so that it's keyed to
		 * the SPI in the current packet.
		 */
		if (direction == PF_IN) {
			key.gwy.xport.spi = 0;

			s = pf_find_state(kif, &key, direction);
			if (s) {
				struct pf_state_key *sk = s->state_key;

				pf_remove_state_key_ext_gwy(sk);
				sk->lan.xport.spi = sk->gwy.xport.spi =
				    esp->spi;

				if (pf_insert_state_key_ext_gwy(sk)) {
					pf_detach_state(s, PF_DT_SKIP_EXTGWY);
				} else {
					*state = s;
				}
			}
		} else {
			key.ext_lan.xport.spi = 0;

			s = pf_find_state(kif, &key, direction);
			if (s) {
				struct pf_state_key *sk = s->state_key;

				RB_REMOVE(pf_state_tree_lan_ext,
				    &pf_statetbl_lan_ext, sk);
				sk->ext_lan.xport.spi = esp->spi;

				if (RB_INSERT(pf_state_tree_lan_ext,
				    &pf_statetbl_lan_ext, sk)) {
					pf_detach_state(s, PF_DT_SKIP_LANEXT);
				} else {
					*state = s;
				}
			}
		}

		if (s) {
			if (*state == 0) {
#if NPFSYNC
				if (s->creatorid == pf_status.hostid) {
					pfsync_delete_state(s);
				}
#endif
				s->timeout = PFTM_UNLINKED;
				hook_runloop(&s->unlink_hooks,
				    HOOK_REMOVE | HOOK_FREE);
				pf_src_tree_remove_state(s);
				pf_free_state(s);
				return PF_DROP;
			}
		}
	}

	/* similar to STATE_LOOKUP() */
	if (*state != NULL && pd != NULL && !(pd->pktflags & PKTF_FLOW_ID)) {
		pd->flowsrc = (*state)->state_key->flowsrc;
		pd->flowhash = (*state)->state_key->flowhash;
		if (pd->flowhash != 0) {
			pd->pktflags |= PKTF_FLOW_ID;
			pd->pktflags &= ~PKTF_FLOW_ADV;
		}
	}

	if (pf_state_lookup_aux(state, kif, direction, &action)) {
		return action;
	}

	if (direction == (*state)->state_key->direction) {
		src = &(*state)->src;
		dst = &(*state)->dst;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

	/* update states */
	if (src->state < PFESPS_INITIATING) {
		src->state = PFESPS_INITIATING;
	}

	/* update expire time */
	(*state)->expire = pf_time_second();
	if (src->state >= PFESPS_INITIATING &&
	    dst->state >= PFESPS_INITIATING) {
		(*state)->timeout = PFTM_ESP_ESTABLISHED;
		src->state = PFESPS_ESTABLISHED;
		dst->state = PFESPS_ESTABLISHED;
	} else {
		(*state)->timeout = PFTM_ESP_INITIATING;
	}
	/* translate source/destination address, if necessary */
	if (STATE_ADDR_TRANSLATE((*state)->state_key)) {
		if (direction == PF_OUT) {
			switch (pd->af) {
#if INET
			case AF_INET:
				pf_change_a(&pd->src->v4addr.s_addr,
				    pd->ip_sum,
				    (*state)->state_key->gwy.addr.v4addr.s_addr, 0);
				break;
#endif /* INET */
			case AF_INET6:
				PF_ACPY(pd->src, &(*state)->state_key->gwy.addr,
				    pd->af);
				break;
			}
		} else {
			switch (pd->af) {
#if INET
			case AF_INET:
				pf_change_a(&pd->dst->v4addr.s_addr,
				    pd->ip_sum,
				    (*state)->state_key->lan.addr.v4addr.s_addr, 0);
				break;
#endif /* INET */
			case AF_INET6:
				PF_ACPY(pd->dst, &(*state)->state_key->lan.addr,
				    pd->af);
				break;
			}
		}
	}

	return PF_PASS;
}

static __attribute__((noinline)) int
pf_test_state_other(struct pf_state **state, int direction, struct pfi_kif *kif,
    struct pf_pdesc *pd)
{
	struct pf_state_peer    *src, *dst;
	struct pf_state_key_cmp  key = {};

	key.app_state = 0;
	key.proto = pd->proto;
	if (direction == PF_IN) {
		key.af_gwy = pd->af;
		PF_ACPY(&key.ext_gwy.addr, pd->src, key.af_gwy);
		PF_ACPY(&key.gwy.addr, pd->dst, key.af_gwy);
		key.ext_gwy.xport.port = 0;
		key.gwy.xport.port = 0;
	} else {
		key.af_lan = pd->af;
		PF_ACPY(&key.lan.addr, pd->src, key.af_lan);
		PF_ACPY(&key.ext_lan.addr, pd->dst, key.af_lan);
		key.lan.xport.port = 0;
		key.ext_lan.xport.port = 0;
	}

	STATE_LOOKUP();

	if (direction == (*state)->state_key->direction) {
		src = &(*state)->src;
		dst = &(*state)->dst;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

	/* update states */
	if (src->state < PFOTHERS_SINGLE) {
		src->state = PFOTHERS_SINGLE;
	}
	if (dst->state == PFOTHERS_SINGLE) {
		dst->state = PFOTHERS_MULTIPLE;
	}

	/* update expire time */
	(*state)->expire = pf_time_second();
	if (src->state == PFOTHERS_MULTIPLE && dst->state == PFOTHERS_MULTIPLE) {
		(*state)->timeout = PFTM_OTHER_MULTIPLE;
	} else {
		(*state)->timeout = PFTM_OTHER_SINGLE;
	}

	/* translate source/destination address, if necessary */
	if (STATE_ADDR_TRANSLATE((*state)->state_key)) {
		if (direction == PF_OUT) {
			switch (pd->af) {
#if INET
			case AF_INET:
				pf_change_a(&pd->src->v4addr.s_addr,
				    pd->ip_sum,
				    (*state)->state_key->gwy.addr.v4addr.s_addr,
				    0);
				break;
#endif /* INET */
			case AF_INET6:
				PF_ACPY(pd->src,
				    &(*state)->state_key->gwy.addr, pd->af);
				break;
			}
		} else {
			switch (pd->af) {
#if INET
			case AF_INET:
				pf_change_a(&pd->dst->v4addr.s_addr,
				    pd->ip_sum,
				    (*state)->state_key->lan.addr.v4addr.s_addr,
				    0);
				break;
#endif /* INET */
			case AF_INET6:
				PF_ACPY(pd->dst,
				    &(*state)->state_key->lan.addr, pd->af);
				break;
			}
		}
	}

	return PF_PASS;
}

/*
 * ipoff and off are measured from the start of the mbuf chain.
 * h must be at "ipoff" on the mbuf chain.
 */
void *
pf_pull_hdr(pbuf_t *pbuf, int off, void *__sized_by(p_buflen)p, int p_buflen, int copylen,
    u_short *actionp, u_short *reasonp, sa_family_t af)
{
	switch (af) {
#if INET
	case AF_INET: {
		struct ip       *__single h = pbuf->pb_data;
		u_int16_t        fragoff = (ntohs(h->ip_off) & IP_OFFMASK) << 3;

		if (fragoff) {
			if (fragoff >= copylen) {
				ACTION_SET(actionp, PF_PASS);
			} else {
				ACTION_SET(actionp, PF_DROP);
				REASON_SET(reasonp, PFRES_FRAG);
			}
			return NULL;
		}
		if (pbuf->pb_packet_len < (unsigned)(off + copylen) ||
		    ntohs(h->ip_len) < off + copylen) {
			ACTION_SET(actionp, PF_DROP);
			REASON_SET(reasonp, PFRES_SHORT);
			return NULL;
		}
		break;
	}
#endif /* INET */
	case AF_INET6: {
		struct ip6_hdr  *__single h = pbuf->pb_data;

		if (pbuf->pb_packet_len < (unsigned)(off + copylen) ||
		    (ntohs(h->ip6_plen) + sizeof(struct ip6_hdr)) <
		    (unsigned)(off + copylen)) {
			ACTION_SET(actionp, PF_DROP);
			REASON_SET(reasonp, PFRES_SHORT);
			return NULL;
		}
		break;
	}
	}
	pbuf_copy_data(pbuf, off, copylen, p, p_buflen);
	return p;
}

int
pf_routable(struct pf_addr *addr, sa_family_t af, struct pfi_kif *kif)
{
#pragma unused(kif)
	struct sockaddr_in      *dst;
	int                      ret = 1;
	struct sockaddr_in6     *dst6;
	struct route_in6         ro;

	bzero(&ro, sizeof(ro));
	switch (af) {
	case AF_INET:
		dst = satosin(&ro.ro_dst);
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = addr->v4addr;
		break;
	case AF_INET6:
		dst6 = (struct sockaddr_in6 *)&ro.ro_dst;
		dst6->sin6_family = AF_INET6;
		dst6->sin6_len = sizeof(*dst6);
		dst6->sin6_addr = addr->v6addr;
		break;
	default:
		return 0;
	}

	/* XXX: IFT_ENC is not currently used by anything*/
	/* Skip checks for ipsec interfaces */
	if (kif != NULL && kif->pfik_ifp->if_type == IFT_ENC) {
		goto out;
	}

	/* XXX: what is the point of this? */
	rtalloc((struct route *)&ro);

out:
	ROUTE_RELEASE(&ro);
	return ret;
}

int
pf_rtlabel_match(struct pf_addr *addr, sa_family_t af, struct pf_addr_wrap *aw)
{
#pragma unused(aw)
	struct sockaddr_in      *dst;
	struct sockaddr_in6     *dst6;
	struct route_in6         ro;
	int                      ret = 0;

	bzero(&ro, sizeof(ro));
	switch (af) {
	case AF_INET:
		dst = satosin(&ro.ro_dst);
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = addr->v4addr;
		break;
	case AF_INET6:
		dst6 = (struct sockaddr_in6 *)&ro.ro_dst;
		dst6->sin6_family = AF_INET6;
		dst6->sin6_len = sizeof(*dst6);
		dst6->sin6_addr = addr->v6addr;
		break;
	default:
		return 0;
	}

	/* XXX: what is the point of this? */
	rtalloc((struct route *)&ro);

	ROUTE_RELEASE(&ro);

	return ret;
}

#if INET
static __attribute__((noinline)) void
pf_route(pbuf_t **pbufp, struct pf_rule *r, int dir, struct ifnet *oifp,
    struct pf_state *s, struct pf_pdesc *pd)
{
#pragma unused(pd)
	struct mbuf             *__single m0, *__single m1;
	struct route             iproute;
	struct route            *__single ro = &iproute;
	struct sockaddr_in      *__single dst;
	struct ip               *__single ip;
	struct ifnet            *__single ifp = NULL;
	struct pf_addr           naddr;
	struct pf_src_node      *__single sn = NULL;
	int                      error = 0;
	uint32_t                 sw_csum;
	int                      interface_mtu = 0;
	bzero(&iproute, sizeof(iproute));

	if (pbufp == NULL || !pbuf_is_valid(*pbufp) || r == NULL ||
	    (dir != PF_IN && dir != PF_OUT) || oifp == NULL) {
		panic("pf_route: invalid parameters");
	}

	if (pd->pf_mtag->pftag_routed++ > 3) {
		pbuf_destroy(*pbufp);
		*pbufp = NULL;
		m0 = NULL;
		goto bad;
	}

	/*
	 * Since this is something of an edge case and may involve the
	 * host stack (for routing, at least for now), we convert the
	 * incoming pbuf into an mbuf.
	 */
	if (r->rt == PF_DUPTO) {
		m0 = pbuf_clone_to_mbuf(*pbufp);
	} else if ((r->rt == PF_REPLYTO) == (r->direction == dir)) {
		return;
	} else {
		/* We're going to consume this packet */
		m0 = pbuf_to_mbuf(*pbufp, TRUE);
		*pbufp = NULL;
	}

	if (m0 == NULL) {
		goto bad;
	}

	/* We now have the packet in an mbuf (m0) */

	if (m0->m_len < (int)sizeof(struct ip)) {
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_route: packet length < sizeof (struct ip)\n"));
		goto bad;
	}

	ip = mtod(m0, struct ip *);

	dst = satosin((void *)&ro->ro_dst);
	dst->sin_family = AF_INET;
	dst->sin_len = sizeof(*dst);
	dst->sin_addr = ip->ip_dst;

	if (r->rt == PF_FASTROUTE) {
		rtalloc(ro);
		if (ro->ro_rt == NULL) {
			ipstat.ips_noroute++;
			goto bad;
		}

		ifp = ro->ro_rt->rt_ifp;
		RT_LOCK(ro->ro_rt);
		ro->ro_rt->rt_use++;

		if (ro->ro_rt->rt_flags & RTF_GATEWAY) {
			dst = satosin((void *)ro->ro_rt->rt_gateway);
		}
		RT_UNLOCK(ro->ro_rt);
	} else {
		if (TAILQ_EMPTY(&r->rpool.list)) {
			DPFPRINTF(PF_DEBUG_URGENT,
			    ("pf_route: TAILQ_EMPTY(&r->rpool.list)\n"));
			goto bad;
		}
		if (s == NULL) {
			pf_map_addr(AF_INET, r, (struct pf_addr *)&ip->ip_src,
			    &naddr, NULL, &sn);
			if (!PF_AZERO(&naddr, AF_INET)) {
				dst->sin_addr.s_addr = naddr.v4addr.s_addr;
			}
			ifp = r->rpool.cur->kif ?
			    r->rpool.cur->kif->pfik_ifp : NULL;
		} else {
			if (!PF_AZERO(&s->rt_addr, AF_INET)) {
				dst->sin_addr.s_addr =
				    s->rt_addr.v4addr.s_addr;
			}
			ifp = s->rt_kif ? s->rt_kif->pfik_ifp : NULL;
		}
	}
	if (ifp == NULL) {
		goto bad;
	}

	if (oifp != ifp) {
		if (pf_test_mbuf(PF_OUT, ifp, &m0, NULL, NULL) != PF_PASS) {
			goto bad;
		} else if (m0 == NULL) {
			goto done;
		}
		if (m0->m_len < (int)sizeof(struct ip)) {
			DPFPRINTF(PF_DEBUG_URGENT,
			    ("pf_route: packet length < sizeof (struct ip)\n"));
			goto bad;
		}
		ip = mtod(m0, struct ip *);
	}

	/* Catch routing changes wrt. hardware checksumming for TCP or UDP. */
	ip_output_checksum(ifp, m0, ((ip->ip_hl) << 2), ntohs(ip->ip_len),
	    &sw_csum);

	interface_mtu = ifp->if_mtu;

	if (INTF_ADJUST_MTU_FOR_CLAT46(ifp)) {
		interface_mtu = IN6_LINKMTU(ifp);
		/* Further adjust the size for CLAT46 expansion */
		interface_mtu -= CLAT46_HDR_EXPANSION_OVERHD;
	}

	if (ntohs(ip->ip_len) <= interface_mtu || TSO_IPV4_OK(ifp, m0) ||
	    (!(ip->ip_off & htons(IP_DF)) &&
	    (ifp->if_hwassist & CSUM_FRAGMENT))) {
		ip->ip_sum = 0;
		if (sw_csum & CSUM_DELAY_IP) {
			ip->ip_sum = in_cksum(m0, ip->ip_hl << 2);
			sw_csum &= ~CSUM_DELAY_IP;
			m0->m_pkthdr.csum_flags &= ~CSUM_DELAY_IP;
		}
		error = ifnet_output(ifp, PF_INET, m0, ro->ro_rt, sintosa(dst));
		goto done;
	}

	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 * Balk when DF bit is set or the interface didn't support TSO.
	 */
	if ((ip->ip_off & htons(IP_DF)) ||
	    (m0->m_pkthdr.csum_flags & CSUM_TSO_IPV4)) {
		ipstat.ips_cantfrag++;
		if (r->rt != PF_DUPTO) {
			icmp_error(m0, ICMP_UNREACH, ICMP_UNREACH_NEEDFRAG, 0,
			    interface_mtu);
			goto done;
		} else {
			goto bad;
		}
	}

	m1 = m0;

	/* PR-8933605: send ip_len,ip_off to ip_fragment in host byte order */
#if BYTE_ORDER != BIG_ENDIAN
	NTOHS(ip->ip_off);
	NTOHS(ip->ip_len);
#endif
	error = ip_fragment(m0, ifp, interface_mtu, sw_csum);

	if (error) {
		m0 = NULL;
		goto bad;
	}

	for (m0 = m1; m0; m0 = m1) {
		m1 = m0->m_nextpkt;
		m0->m_nextpkt = 0;
		if (error == 0) {
			error = ifnet_output(ifp, PF_INET, m0, ro->ro_rt,
			    sintosa(dst));
		} else {
			m_freem(m0);
		}
	}

	if (error == 0) {
		ipstat.ips_fragmented++;
	}

done:
	ROUTE_RELEASE(&iproute);
	return;

bad:
	if (m0) {
		m_freem(m0);
	}
	goto done;
}
#endif /* INET */

static __attribute__((noinline)) void
pf_route6(pbuf_t **pbufp, struct pf_rule *r, int dir, struct ifnet *oifp,
    struct pf_state *s, struct pf_pdesc *pd)
{
#pragma unused(pd)
	struct mbuf             *__single m0;
	struct route_in6         ip6route;
	struct route_in6        *__single ro;
	struct sockaddr_in6     *__single dst;
	struct ip6_hdr          *__single ip6;
	struct ifnet            *__single ifp = NULL;
	struct pf_addr           naddr;
	struct pf_src_node      *__single sn = NULL;
	int                      error = 0;
	struct pf_mtag          *__single pf_mtag;

	if (pbufp == NULL || !pbuf_is_valid(*pbufp) || r == NULL ||
	    (dir != PF_IN && dir != PF_OUT) || oifp == NULL) {
		panic("pf_route6: invalid parameters");
	}

	if (pd->pf_mtag->pftag_routed++ > 3) {
		pbuf_destroy(*pbufp);
		*pbufp = NULL;
		m0 = NULL;
		goto bad;
	}

	/*
	 * Since this is something of an edge case and may involve the
	 * host stack (for routing, at least for now), we convert the
	 * incoming pbuf into an mbuf.
	 */
	if (r->rt == PF_DUPTO) {
		m0 = pbuf_clone_to_mbuf(*pbufp);
	} else if ((r->rt == PF_REPLYTO) == (r->direction == dir)) {
		return;
	} else {
		/* We're about to consume this packet */
		m0 = pbuf_to_mbuf(*pbufp, TRUE);
		*pbufp = NULL;
	}

	if (m0 == NULL) {
		goto bad;
	}

	if (m0->m_len < (int)sizeof(struct ip6_hdr)) {
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_route6: m0->m_len < sizeof (struct ip6_hdr)\n"));
		goto bad;
	}
	ip6 = mtod(m0, struct ip6_hdr *);

	ro = &ip6route;
	bzero((void *__bidi_indexable)(struct route_in6 *__bidi_indexable)ro, sizeof(*ro));
	dst = SIN6(&ro->ro_dst);
	dst->sin6_family = AF_INET6;
	dst->sin6_len = sizeof(*dst);
	dst->sin6_addr = ip6->ip6_dst;

	/* Cheat. XXX why only in the v6addr case??? */
	if (r->rt == PF_FASTROUTE) {
		pf_mtag = pf_get_mtag(m0);
		ASSERT(pf_mtag != NULL);
		pf_mtag->pftag_flags |= PF_TAG_GENERATED;
		ip6_output_setsrcifscope(m0, oifp->if_index, NULL);
		ip6_output_setdstifscope(m0, oifp->if_index, NULL);
		ip6_output(m0, NULL, NULL, 0, NULL, NULL, NULL);
		return;
	}

	if (TAILQ_EMPTY(&r->rpool.list)) {
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_route6: TAILQ_EMPTY(&r->rpool.list)\n"));
		goto bad;
	}
	if (s == NULL) {
		pf_map_addr(AF_INET6, r, (struct pf_addr *)(void *)&ip6->ip6_src,
		    &naddr, NULL, &sn);
		if (!PF_AZERO(&naddr, AF_INET6)) {
			PF_ACPY((struct pf_addr *)&dst->sin6_addr,
			    &naddr, AF_INET6);
		}
		ifp = r->rpool.cur->kif ? r->rpool.cur->kif->pfik_ifp : NULL;
	} else {
		if (!PF_AZERO(&s->rt_addr, AF_INET6)) {
			PF_ACPY((struct pf_addr *)&dst->sin6_addr,
			    &s->rt_addr, AF_INET6);
		}
		ifp = s->rt_kif ? s->rt_kif->pfik_ifp : NULL;
	}
	if (ifp == NULL) {
		goto bad;
	}

	if (oifp != ifp) {
		if (pf_test6_mbuf(PF_OUT, ifp, &m0, NULL, NULL) != PF_PASS) {
			goto bad;
		} else if (m0 == NULL) {
			goto done;
		}
		if (m0->m_len < (int)sizeof(struct ip6_hdr)) {
			DPFPRINTF(PF_DEBUG_URGENT, ("pf_route6: m0->m_len "
			    "< sizeof (struct ip6_hdr)\n"));
			goto bad;
		}
		pf_mtag = pf_get_mtag(m0);
		/*
		 * send refragmented packets.
		 */
		if ((pf_mtag->pftag_flags & PF_TAG_REFRAGMENTED) != 0) {
			pf_mtag->pftag_flags &= ~PF_TAG_REFRAGMENTED;
			/*
			 * nd6_output() frees packet chain in both success and
			 * failure cases.
			 */
			error = nd6_output(ifp, ifp, m0, dst, NULL, NULL);
			m0 = NULL;
			if (error) {
				DPFPRINTF(PF_DEBUG_URGENT, ("pf_route6:"
				    "dropped refragmented packet\n"));
			}
			goto done;
		}
		ip6 = mtod(m0, struct ip6_hdr *);
	}

	/*
	 * If the packet is too large for the outgoing interface,
	 * send back an icmp6 error.
	 */
	if (in6_embedded_scope && IN6_IS_SCOPE_EMBED(&dst->sin6_addr)) {
		dst->sin6_addr.s6_addr16[1] = htons(ifp->if_index);
	}
	if ((unsigned)m0->m_pkthdr.len <= ifp->if_mtu) {
		error = nd6_output(ifp, ifp, m0, dst, NULL, NULL);
	} else {
		in6_ifstat_inc(ifp, ifs6_in_toobig);
		if (r->rt != PF_DUPTO) {
			icmp6_error(m0, ICMP6_PACKET_TOO_BIG, 0, ifp->if_mtu);
		} else {
			goto bad;
		}
	}

done:
	return;

bad:
	if (m0) {
		m_freem(m0);
		m0 = NULL;
	}
	goto done;
}


/*
 * check protocol (tcp/udp/icmp/icmp6) checksum and set mbuf flag
 *   off is the offset where the protocol header starts
 *   len is the total length of protocol header plus payload
 * returns 0 when the checksum is valid, otherwise returns 1.
 */
static int
pf_check_proto_cksum(pbuf_t *pbuf, int off, int len, u_int8_t p,
    sa_family_t af)
{
	u_int16_t sum;

	switch (p) {
	case IPPROTO_TCP:
	case IPPROTO_UDP:
		/*
		 * Optimize for the common case; if the hardware calculated
		 * value doesn't include pseudo-header checksum, or if it
		 * is partially-computed (only 16-bit summation), do it in
		 * software below.
		 */
		if ((*pbuf->pb_csum_flags &
		    (CSUM_DATA_VALID | CSUM_PSEUDO_HDR)) ==
		    (CSUM_DATA_VALID | CSUM_PSEUDO_HDR) &&
		    (*pbuf->pb_csum_data ^ 0xffff) == 0) {
			return 0;
		}
		break;
	case IPPROTO_ICMP:
	case IPPROTO_ICMPV6:
		break;
	default:
		return 1;
	}
	if (off < (int)sizeof(struct ip) || len < (int)sizeof(struct udphdr)) {
		return 1;
	}
	if (pbuf->pb_packet_len < (unsigned)(off + len)) {
		return 1;
	}
	switch (af) {
#if INET
	case AF_INET:
		if (p == IPPROTO_ICMP) {
			if (pbuf->pb_contig_len < (unsigned)off) {
				return 1;
			}
			sum = pbuf_inet_cksum(pbuf, 0, off, len);
		} else {
			if (pbuf->pb_contig_len < (int)sizeof(struct ip)) {
				return 1;
			}
			sum = pbuf_inet_cksum(pbuf, p, off, len);
		}
		break;
#endif /* INET */
	case AF_INET6:
		if (pbuf->pb_contig_len < (int)sizeof(struct ip6_hdr)) {
			return 1;
		}
		sum = pbuf_inet6_cksum(pbuf, p, off, len);
		break;
	default:
		return 1;
	}
	if (sum) {
		switch (p) {
		case IPPROTO_TCP:
			tcpstat.tcps_rcvbadsum++;
			break;
		case IPPROTO_UDP:
			udpstat.udps_badsum++;
			break;
		case IPPROTO_ICMP:
			icmpstat.icps_checksum++;
			break;
		case IPPROTO_ICMPV6:
			icmp6stat.icp6s_checksum++;
			break;
		}
		return 1;
	}
	return 0;
}

#if INET
#define PF_APPLE_UPDATE_PDESC_IPv4()                            \
	do {                                                    \
	        if (pbuf && pd.mp && pbuf != pd.mp) {           \
	                pbuf = pd.mp;                           \
	                h = pbuf->pb_data;                      \
	                pd.pf_mtag = pf_get_mtag_pbuf(pbuf);            \
	        }                                               \
	} while (0)

int
pf_test_mbuf(int dir, struct ifnet *ifp, struct mbuf **m0,
    struct ether_header *eh, struct ip_fw_args *fwa)
{
	pbuf_t pbuf_store, *__single pbuf;
	int rv;

	pbuf_init_mbuf(&pbuf_store, *m0, (*m0)->m_pkthdr.rcvif);
	pbuf = &pbuf_store;

	rv = pf_test(dir, ifp, &pbuf, eh, fwa);

	if (pbuf_is_valid(pbuf)) {
		*m0 = pbuf->pb_mbuf;
		pbuf->pb_mbuf = NULL;
		pbuf_destroy(pbuf);
	} else {
		*m0 = NULL;
	}

	return rv;
}

static __attribute__((noinline)) int
pf_test(int dir, struct ifnet *ifp, pbuf_t **pbufp,
    struct ether_header *eh, struct ip_fw_args *fwa)
{
#if !DUMMYNET
#pragma unused(fwa)
#endif
	struct pfi_kif          *__single kif;
	u_short                  action = PF_PASS, reason = 0, log = 0;
	pbuf_t                  *__single pbuf = *pbufp;
	struct ip               *__single h = 0;
	struct pf_rule          *__single a = NULL, *__single r = &pf_default_rule, *__single tr, *__single nr;
	struct pf_state         *__single s = NULL;
	struct pf_state_key     *__single sk = NULL;
	struct pf_ruleset       *__single ruleset = NULL;
	struct pf_pdesc          pd;
	int                      off, dirndx, pqid = 0;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	if (!pf_status.running) {
		return PF_PASS;
	}

	memset(&pd, 0, sizeof(pd));

	if ((pd.pf_mtag = pf_get_mtag_pbuf(pbuf)) == NULL) {
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_test: pf_get_mtag_pbuf returned NULL\n"));
		return PF_DROP;
	}

	if (pd.pf_mtag->pftag_flags & PF_TAG_GENERATED) {
		return PF_PASS;
	}

	kif = (struct pfi_kif *)ifp->if_pf_kif;

	if (kif == NULL) {
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_test: kif == NULL, if_name %s\n", ifp->if_name));
		return PF_DROP;
	}
	if (kif->pfik_flags & PFI_IFLAG_SKIP) {
		return PF_PASS;
	}

	if (pbuf->pb_packet_len < (int)sizeof(*h)) {
		REASON_SET(&reason, PFRES_SHORT);
		return PF_DROP;
	}

	/* initialize enough of pd for the done label */
	h = pbuf->pb_data;
	pd.mp = pbuf;
	pd.lmw = 0;
	pd.pf_mtag = pf_get_mtag_pbuf(pbuf);
	pd.src = (struct pf_addr *)&h->ip_src;
	pd.dst = (struct pf_addr *)&h->ip_dst;
	PF_ACPY(&pd.baddr, pd.src, AF_INET);
	PF_ACPY(&pd.bdaddr, pd.dst, AF_INET);
	pd.ip_sum = &h->ip_sum;
	pd.proto = h->ip_p;
	pd.proto_variant = 0;
	pd.af = AF_INET;
	pd.tos = h->ip_tos;
	pd.ttl = h->ip_ttl;
	pd.tot_len = ntohs(h->ip_len);
	pd.eh = eh;

#if DUMMYNET
	if (fwa != NULL && fwa->fwa_pf_rule != NULL) {
		goto nonormalize;
	}
#endif /* DUMMYNET */

	/* We do IP header normalization and packet reassembly here */
	action = pf_normalize_ip(pbuf, dir, kif, &reason, &pd);
	if (action != PF_PASS || pd.lmw < 0) {
		action = PF_DROP;
		goto done;
	}

#if DUMMYNET
nonormalize:
#endif /* DUMMYNET */
	/* pf_normalize can mess with pb_data */
	h = pbuf->pb_data;

	off = h->ip_hl << 2;
	if (off < (int)sizeof(*h)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_SHORT);
		log = 1;
		goto done;
	}

	pd.src = (struct pf_addr *)&h->ip_src;
	pd.dst = (struct pf_addr *)&h->ip_dst;
	PF_ACPY(&pd.baddr, pd.src, AF_INET);
	PF_ACPY(&pd.bdaddr, pd.dst, AF_INET);
	pd.ip_sum = &h->ip_sum;
	pd.proto = h->ip_p;
	pd.proto_variant = 0;
	pd.mp = pbuf;
	pd.lmw = 0;
	pd.pf_mtag = pf_get_mtag_pbuf(pbuf);
	pd.af = AF_INET;
	pd.tos = h->ip_tos;
	pd.ttl = h->ip_ttl;
	pd.sc = MBUF_SCIDX(pbuf_get_service_class(pbuf));
	pd.tot_len = ntohs(h->ip_len);
	pd.eh = eh;

	if (*pbuf->pb_flags & PKTF_FLOW_ID) {
		pd.flowsrc = *pbuf->pb_flowsrc;
		pd.flowhash = *pbuf->pb_flowid;
		pd.pktflags = *pbuf->pb_flags & PKTF_FLOW_MASK;
	}

	/* handle fragments that didn't get reassembled by normalization */
	if (h->ip_off & htons(IP_MF | IP_OFFMASK)) {
		pd.flags |= PFDESC_IP_FRAG;
#if DUMMYNET
		/* Traffic goes through dummynet first */
		action = pf_test_dummynet(&r, dir, kif, &pbuf, &pd, fwa);
		if (action == PF_DROP || pbuf == NULL) {
			*pbufp = NULL;
			return action;
		}
#endif /* DUMMYNET */
		action = pf_test_fragment(&r, dir, kif, pbuf, h,
		    &pd, &a, &ruleset);
		goto done;
	}

	switch (h->ip_p) {
	case IPPROTO_TCP: {
		struct tcphdr   th;
		pf_pd_set_hdr_tcp(&pd, &th);
		if (!pf_pull_hdr(pbuf, off, &th, sizeof(th), sizeof(th),
		    &action, &reason, AF_INET)) {
			log = action != PF_PASS;
			goto done;
		}
		pd.p_len = pd.tot_len - off - (th.th_off << 2);
		if ((th.th_flags & TH_ACK) && pd.p_len == 0) {
			pqid = 1;
		}
#if DUMMYNET
		/* Traffic goes through dummynet first */
		action = pf_test_dummynet(&r, dir, kif, &pbuf, &pd, fwa);
		if (action == PF_DROP || pbuf == NULL) {
			*pbufp = NULL;
			return action;
		}
#endif /* DUMMYNET */
		action = pf_normalize_tcp(dir, kif, pbuf, 0, off, h, &pd);
		if (pd.lmw < 0) {
			goto done;
		}
		PF_APPLE_UPDATE_PDESC_IPv4();
		if (action == PF_DROP) {
			goto done;
		}
		if (th.th_sport == 0 || th.th_dport == 0) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_INVPORT);
			goto done;
		}
		action = pf_test_state_tcp(&s, dir, kif, pbuf, off, h, &pd,
		    &reason);
		if (action == PF_NAT64) {
			goto done;
		}
		if (pd.lmw < 0) {
			goto done;
		}
		PF_APPLE_UPDATE_PDESC_IPv4();
		if (action == PF_PASS) {
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL) {
			action = pf_test_rule(&r, &s, dir, kif,
			    pbuf, off, h, &pd, &a, &ruleset, NULL);
		}
		break;
	}

	case IPPROTO_UDP: {
		struct udphdr   uh;

		pf_pd_set_hdr_udp(&pd, &uh);
		if (!pf_pull_hdr(pbuf, off, &uh, sizeof(uh), sizeof(uh),
		    &action, &reason, AF_INET)) {
			log = action != PF_PASS;
			goto done;
		}
		if (uh.uh_sport == 0 || uh.uh_dport == 0) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_INVPORT);
			goto done;
		}
		if (ntohs(uh.uh_ulen) > pbuf->pb_packet_len - off ||
		    ntohs(uh.uh_ulen) < sizeof(struct udphdr)) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_SHORT);
			goto done;
		}
#if DUMMYNET
		/* Traffic goes through dummynet first */
		action = pf_test_dummynet(&r, dir, kif, &pbuf, &pd, fwa);
		if (action == PF_DROP || pbuf == NULL) {
			*pbufp = NULL;
			return action;
		}
#endif /* DUMMYNET */
		action = pf_test_state_udp(&s, dir, kif, pbuf, off, h, &pd,
		    &reason);
		if (action == PF_NAT64) {
			goto done;
		}
		if (pd.lmw < 0) {
			goto done;
		}
		PF_APPLE_UPDATE_PDESC_IPv4();
		if (action == PF_PASS) {
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL) {
			action = pf_test_rule(&r, &s, dir, kif,
			    pbuf, off, h, &pd, &a, &ruleset, NULL);
		}
		break;
	}

	case IPPROTO_ICMP: {
		struct icmp     ih;

		pf_pd_set_hdr_icmp(&pd, &ih, ICMP_MINLEN);
		if (!pf_pull_hdr(pbuf, off, &ih, sizeof(ih), ICMP_MINLEN,
		    &action, &reason, AF_INET)) {
			log = action != PF_PASS;
			goto done;
		}
#if DUMMYNET
		/* Traffic goes through dummynet first */
		action = pf_test_dummynet(&r, dir, kif, &pbuf, &pd, fwa);
		if (action == PF_DROP || pbuf == NULL) {
			*pbufp = NULL;
			return action;
		}
#endif /* DUMMYNET */
		action = pf_test_state_icmp(&s, dir, kif, pbuf, off, h, &pd,
		    &reason);

		if (action == PF_NAT64) {
			goto done;
		}
		if (pd.lmw < 0) {
			goto done;
		}
		PF_APPLE_UPDATE_PDESC_IPv4();
		if (action == PF_PASS) {
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL) {
			action = pf_test_rule(&r, &s, dir, kif,
			    pbuf, off, h, &pd, &a, &ruleset, NULL);
		}
		break;
	}

	case IPPROTO_ESP: {
		struct pf_esp_hdr       esp;

		pf_pd_set_hdr_esp(&pd, &esp);
		if (!pf_pull_hdr(pbuf, off, &esp, sizeof(esp), sizeof(esp), &action, &reason,
		    AF_INET)) {
			log = action != PF_PASS;
			goto done;
		}
#if DUMMYNET
		/* Traffic goes through dummynet first */
		action = pf_test_dummynet(&r, dir, kif, &pbuf, &pd, fwa);
		if (action == PF_DROP || pbuf == NULL) {
			*pbufp = NULL;
			return action;
		}
#endif /* DUMMYNET */
		action = pf_test_state_esp(&s, dir, kif, off, &pd);
		if (pd.lmw < 0) {
			goto done;
		}
		PF_APPLE_UPDATE_PDESC_IPv4();
		if (action == PF_PASS) {
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL) {
			action = pf_test_rule(&r, &s, dir, kif,
			    pbuf, off, h, &pd, &a, &ruleset, NULL);
		}
		break;
	}

	case IPPROTO_GRE: {
		struct pf_grev1_hdr     grev1;
		pf_pd_set_hdr_grev1(&pd, &grev1);
		if (!pf_pull_hdr(pbuf, off, &grev1, sizeof(grev1), sizeof(grev1), &action,
		    &reason, AF_INET)) {
			log = (action != PF_PASS);
			goto done;
		}
#if DUMMYNET
		/* Traffic goes through dummynet first */
		action = pf_test_dummynet(&r, dir, kif, &pbuf, &pd, fwa);
		if (action == PF_DROP || pbuf == NULL) {
			*pbufp = NULL;
			return action;
		}
#endif /* DUMMYNET */
		if ((ntohs(grev1.flags) & PF_GRE_FLAG_VERSION_MASK) == 1 &&
		    ntohs(grev1.protocol_type) == PF_GRE_PPP_ETHERTYPE) {
			if (ntohs(grev1.payload_length) >
			    pbuf->pb_packet_len - off) {
				action = PF_DROP;
				REASON_SET(&reason, PFRES_SHORT);
				goto done;
			}
			pd.proto_variant = PF_GRE_PPTP_VARIANT;
			action = pf_test_state_grev1(&s, dir, kif, off, &pd);
			if (pd.lmw < 0) {
				goto done;
			}
			PF_APPLE_UPDATE_PDESC_IPv4();
			if (action == PF_PASS) {
#if NPFSYNC
				pfsync_update_state(s);
#endif /* NPFSYNC */
				r = s->rule.ptr;
				a = s->anchor.ptr;
				log = s->log;
				break;
			} else if (s == NULL) {
				action = pf_test_rule(&r, &s, dir, kif, pbuf,
				    off, h, &pd, &a, &ruleset, NULL);
				if (action == PF_PASS) {
					break;
				}
			}
		}

		/* not GREv1/PPTP, so treat as ordinary GRE... */
		OS_FALLTHROUGH;
	}

	default:
#if DUMMYNET
		/* Traffic goes through dummynet first */
		action = pf_test_dummynet(&r, dir, kif, &pbuf, &pd, fwa);
		if (action == PF_DROP || pbuf == NULL) {
			*pbufp = NULL;
			return action;
		}
#endif /* DUMMYNET */
		action = pf_test_state_other(&s, dir, kif, &pd);
		if (pd.lmw < 0) {
			goto done;
		}
		PF_APPLE_UPDATE_PDESC_IPv4();
		if (action == PF_PASS) {
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL) {
			action = pf_test_rule(&r, &s, dir, kif, pbuf, off, h,
			    &pd, &a, &ruleset, NULL);
		}
		break;
	}

done:
	if (action == PF_NAT64) {
		*pbufp = NULL;
		return action;
	}

	*pbufp = pd.mp;
	PF_APPLE_UPDATE_PDESC_IPv4();

	if (action != PF_DROP) {
		if (action == PF_PASS && h->ip_hl > 5 &&
		    !((s && s->allow_opts) || r->allow_opts)) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_IPOPTIONS);
			log = 1;
			DPFPRINTF(PF_DEBUG_MISC,
			    ("pf: dropping packet with ip options [hlen=%u]\n",
			    (unsigned int) h->ip_hl));
		}

		if ((s && s->tag) || PF_RTABLEID_IS_VALID(r->rtableid) ||
		    (pd.pktflags & PKTF_FLOW_ID)) {
			(void) pf_tag_packet(pbuf, pd.pf_mtag, s ? s->tag : 0,
			    r->rtableid, &pd);
		}

		if (action == PF_PASS) {
#if PF_ECN
			/* add hints for ecn */
			pd.pf_mtag->pftag_hdr = h;
			/* record address family */
			pd.pf_mtag->pftag_flags &= ~PF_TAG_HDR_INET6;
			pd.pf_mtag->pftag_flags |= PF_TAG_HDR_INET;
#endif /* PF_ECN */
			/* record protocol */
			*pbuf->pb_proto = pd.proto;

			/*
			 * connections redirected to loopback should not match sockets
			 * bound specifically to loopback due to security implications,
			 * see tcp_input() and in_pcblookup_listen().
			 */
			if (dir == PF_IN && (pd.proto == IPPROTO_TCP ||
			    pd.proto == IPPROTO_UDP) && s != NULL &&
			    s->nat_rule.ptr != NULL &&
			    (s->nat_rule.ptr->action == PF_RDR ||
			    s->nat_rule.ptr->action == PF_BINAT) &&
			    (ntohl(pd.dst->v4addr.s_addr) >> IN_CLASSA_NSHIFT)
			    == IN_LOOPBACKNET) {
				pd.pf_mtag->pftag_flags |= PF_TAG_TRANSLATE_LOCALHOST;
			}
		}
	}

	if (log) {
		struct pf_rule *lr;

		if (s != NULL && s->nat_rule.ptr != NULL &&
		    s->nat_rule.ptr->log & PF_LOG_ALL) {
			lr = s->nat_rule.ptr;
		} else {
			lr = r;
		}
		PFLOG_PACKET(kif, h, pbuf, AF_INET, dir, reason, lr, a, ruleset,
		    &pd);
	}

	kif->pfik_bytes[0][dir == PF_OUT][action != PF_PASS] += pd.tot_len;
	kif->pfik_packets[0][dir == PF_OUT][action != PF_PASS]++;

	if (action == PF_PASS || r->action == PF_DROP) {
		dirndx = (dir == PF_OUT);
		r->packets[dirndx]++;
		r->bytes[dirndx] += pd.tot_len;
		if (a != NULL) {
			a->packets[dirndx]++;
			a->bytes[dirndx] += pd.tot_len;
		}
		if (s != NULL) {
			sk = s->state_key;
			if (s->nat_rule.ptr != NULL) {
				s->nat_rule.ptr->packets[dirndx]++;
				s->nat_rule.ptr->bytes[dirndx] += pd.tot_len;
			}
			if (s->src_node != NULL) {
				s->src_node->packets[dirndx]++;
				s->src_node->bytes[dirndx] += pd.tot_len;
			}
			if (s->nat_src_node != NULL) {
				s->nat_src_node->packets[dirndx]++;
				s->nat_src_node->bytes[dirndx] += pd.tot_len;
			}
			dirndx = (dir == sk->direction) ? 0 : 1;
			s->packets[dirndx]++;
			s->bytes[dirndx] += pd.tot_len;
		}
		tr = r;
		nr = (s != NULL) ? s->nat_rule.ptr : pd.nat_rule;
		if (nr != NULL) {
			struct pf_addr *x;
			/*
			 * XXX: we need to make sure that the addresses
			 * passed to pfr_update_stats() are the same than
			 * the addresses used during matching (pfr_match)
			 */
			if (r == &pf_default_rule) {
				tr = nr;
				x = (sk == NULL || sk->direction == dir) ?
				    &pd.baddr : &pd.naddr;
			} else {
				x = (sk == NULL || sk->direction == dir) ?
				    &pd.naddr : &pd.baddr;
			}
			if (x == &pd.baddr || s == NULL) {
				/* we need to change the address */
				if (dir == PF_OUT) {
					pd.src = x;
				} else {
					pd.dst = x;
				}
			}
		}
		if (tr->src.addr.type == PF_ADDR_TABLE) {
			pfr_update_stats(tr->src.addr.p.tbl, (sk == NULL ||
			    sk->direction == dir) ?
			    pd.src : pd.dst, pd.af,
			    pd.tot_len, dir == PF_OUT, r->action == PF_PASS,
			    tr->src.neg);
		}
		if (tr->dst.addr.type == PF_ADDR_TABLE) {
			pfr_update_stats(tr->dst.addr.p.tbl, (sk == NULL ||
			    sk->direction == dir) ? pd.dst : pd.src, pd.af,
			    pd.tot_len, dir == PF_OUT, r->action == PF_PASS,
			    tr->dst.neg);
		}
	}

	VERIFY(pbuf == NULL || pd.mp == NULL || pd.mp == pbuf);

	if (*pbufp) {
		if (pd.lmw < 0) {
			REASON_SET(&reason, PFRES_MEMORY);
			action = PF_DROP;
		}

		if (action == PF_DROP) {
			pbuf_destroy(*pbufp);
			*pbufp = NULL;
			return PF_DROP;
		}

		*pbufp = pbuf;
	}

	if (action == PF_SYNPROXY_DROP) {
		pbuf_destroy(*pbufp);
		*pbufp = NULL;
		action = PF_PASS;
	} else if (r->rt) {
		/* pf_route can free the pbuf causing *pbufp to become NULL */
		pf_route(pbufp, r, dir, kif->pfik_ifp, s, &pd);
	}

	return action;
}
#endif /* INET */

#define PF_APPLE_UPDATE_PDESC_IPv6()                            \
	do {                                                    \
	        if (pbuf && pd.mp && pbuf != pd.mp) {           \
	                pbuf = pd.mp;                           \
	        }                                               \
	        h = pbuf->pb_data;                              \
	} while (0)

int
pf_test6_mbuf(int dir, struct ifnet *ifp, struct mbuf **m0,
    struct ether_header *eh, struct ip_fw_args *fwa)
{
	pbuf_t pbuf_store, *__single pbuf;
	int rv;

	pbuf_init_mbuf(&pbuf_store, *m0, (*m0)->m_pkthdr.rcvif);
	pbuf = &pbuf_store;

	rv = pf_test6(dir, ifp, &pbuf, eh, fwa);

	if (pbuf_is_valid(pbuf)) {
		*m0 = pbuf->pb_mbuf;
		pbuf->pb_mbuf = NULL;
		pbuf_destroy(pbuf);
	} else {
		*m0 = NULL;
	}

	return rv;
}

static __attribute__((noinline)) int
pf_test6(int dir, struct ifnet *ifp, pbuf_t **pbufp,
    struct ether_header *eh, struct ip_fw_args *fwa)
{
#if !DUMMYNET
#pragma unused(fwa)
#endif
	struct pfi_kif          *__single kif;
	u_short                  action = PF_PASS, reason = 0, log = 0;
	pbuf_t                  *__single pbuf = *pbufp;
	struct ip6_hdr          *__single h;
	struct pf_rule          *__single a = NULL, *__single r = &pf_default_rule, *__single tr, *__single nr;
	struct pf_state         *__single s = NULL;
	struct pf_state_key     *__single sk = NULL;
	struct pf_ruleset       *__single ruleset = NULL;
	struct pf_pdesc          pd;
	int                      off, terminal = 0, dirndx, rh_cnt = 0;
	u_int8_t                 nxt;
	boolean_t                fwd = FALSE;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	ASSERT(ifp != NULL);
	if ((dir == PF_OUT) && (pbuf->pb_ifp) && (ifp != pbuf->pb_ifp)) {
		fwd = TRUE;
	}

	if (!pf_status.running) {
		return PF_PASS;
	}

	memset(&pd, 0, sizeof(pd));

	if ((pd.pf_mtag = pf_get_mtag_pbuf(pbuf)) == NULL) {
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_test6: pf_get_mtag_pbuf returned NULL\n"));
		return PF_DROP;
	}

	if (pd.pf_mtag->pftag_flags & PF_TAG_GENERATED) {
		return PF_PASS;
	}

	kif = (struct pfi_kif *)ifp->if_pf_kif;

	if (kif == NULL) {
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_test6: kif == NULL, if_name %s\n", ifp->if_name));
		return PF_DROP;
	}
	if (kif->pfik_flags & PFI_IFLAG_SKIP) {
		return PF_PASS;
	}

	if (pbuf->pb_packet_len < (int)sizeof(*h)) {
		REASON_SET(&reason, PFRES_SHORT);
		return PF_DROP;
	}

	h = pbuf->pb_data;
	nxt = h->ip6_nxt;
	off = ((caddr_t)h - (caddr_t)pbuf->pb_data) + sizeof(struct ip6_hdr);
	pd.mp = pbuf;
	pd.lmw = 0;
	pd.pf_mtag = pf_get_mtag_pbuf(pbuf);
	pd.src = (struct pf_addr *)(void *)&h->ip6_src;
	pd.dst = (struct pf_addr *)(void *)&h->ip6_dst;
	PF_ACPY(&pd.baddr, pd.src, AF_INET6);
	PF_ACPY(&pd.bdaddr, pd.dst, AF_INET6);
	pd.ip_sum = NULL;
	pd.af = AF_INET6;
	pd.proto = nxt;
	pd.proto_variant = 0;
	pd.tos = 0;
	pd.ttl = h->ip6_hlim;
	pd.sc = MBUF_SCIDX(pbuf_get_service_class(pbuf));
	pd.tot_len = ntohs(h->ip6_plen) + sizeof(struct ip6_hdr);
	pd.eh = eh;

	if (*pbuf->pb_flags & PKTF_FLOW_ID) {
		pd.flowsrc = *pbuf->pb_flowsrc;
		pd.flowhash = *pbuf->pb_flowid;
		pd.pktflags = (*pbuf->pb_flags & PKTF_FLOW_MASK);
	}

#if DUMMYNET
	if (fwa != NULL && fwa->fwa_pf_rule != NULL) {
		goto nonormalize;
	}
#endif /* DUMMYNET */

	/* We do IP header normalization and packet reassembly here */
	action = pf_normalize_ip6(pbuf, dir, kif, &reason, &pd);
	if (action != PF_PASS || pd.lmw < 0) {
		action = PF_DROP;
		goto done;
	}

#if DUMMYNET
nonormalize:
#endif /* DUMMYNET */
	h = pbuf->pb_data;

	/*
	 * we do not support jumbogram yet.  if we keep going, zero ip6_plen
	 * will do something bad, so drop the packet for now.
	 */
	if (htons(h->ip6_plen) == 0) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_NORM);        /*XXX*/
		goto done;
	}
	pd.src = (struct pf_addr *)(void *)&h->ip6_src;
	pd.dst = (struct pf_addr *)(void *)&h->ip6_dst;
	PF_ACPY(&pd.baddr, pd.src, AF_INET6);
	PF_ACPY(&pd.bdaddr, pd.dst, AF_INET6);
	pd.ip_sum = NULL;
	pd.af = AF_INET6;
	pd.tos = 0;
	pd.ttl = h->ip6_hlim;
	pd.tot_len = ntohs(h->ip6_plen) + sizeof(struct ip6_hdr);
	pd.eh = eh;

	off = ((caddr_t)h - (caddr_t)pbuf->pb_data) + sizeof(struct ip6_hdr);
	pd.proto = h->ip6_nxt;
	pd.proto_variant = 0;
	pd.mp = pbuf;
	pd.lmw = 0;
	pd.pf_mtag = pf_get_mtag_pbuf(pbuf);

	do {
		switch (pd.proto) {
		case IPPROTO_FRAGMENT: {
			struct ip6_frag ip6f;

			pd.flags |= PFDESC_IP_FRAG;
			if (!pf_pull_hdr(pbuf, off, &ip6f, sizeof ip6f, sizeof ip6f, NULL,
			    &reason, pd.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: IPv6 short fragment header\n"));
				action = PF_DROP;
				REASON_SET(&reason, PFRES_SHORT);
				log = 1;
				goto done;
			}
			pd.proto = ip6f.ip6f_nxt;
#if DUMMYNET
			/* Traffic goes through dummynet first */
			action = pf_test_dummynet(&r, dir, kif, &pbuf, &pd,
			    fwa);
			if (action == PF_DROP || pbuf == NULL) {
				*pbufp = NULL;
				return action;
			}
#endif /* DUMMYNET */
			action = pf_test_fragment(&r, dir, kif, pbuf, h, &pd,
			    &a, &ruleset);
			if (action == PF_DROP) {
				REASON_SET(&reason, PFRES_FRAG);
				log = 1;
			}
			goto done;
		}
		case IPPROTO_ROUTING:
			++rh_cnt;
			OS_FALLTHROUGH;

		case IPPROTO_AH:
		case IPPROTO_HOPOPTS:
		case IPPROTO_DSTOPTS: {
			/* get next header and header length */
			struct ip6_ext  opt6;

			if (!pf_pull_hdr(pbuf, off, &opt6, sizeof(opt6), sizeof(opt6),
			    NULL, &reason, pd.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: IPv6 short opt\n"));
				action = PF_DROP;
				log = 1;
				goto done;
			}
			if (pd.proto == IPPROTO_AH) {
				off += (opt6.ip6e_len + 2) * 4;
			} else {
				off += (opt6.ip6e_len + 1) * 8;
			}
			pd.proto = opt6.ip6e_nxt;
			/* goto the next header */
			break;
		}
		default:
			terminal++;
			break;
		}
	} while (!terminal);


	switch (pd.proto) {
	case IPPROTO_TCP: {
		struct tcphdr   th;

		pf_pd_set_hdr_tcp(&pd, &th);
		if (!pf_pull_hdr(pbuf, off, &th, sizeof(th), sizeof(th),
		    &action, &reason, AF_INET6)) {
			log = action != PF_PASS;
			goto done;
		}
		pd.p_len = pd.tot_len - off - (th.th_off << 2);
#if DUMMYNET
		/* Traffic goes through dummynet first */
		action = pf_test_dummynet(&r, dir, kif, &pbuf, &pd, fwa);
		if (action == PF_DROP || pbuf == NULL) {
			*pbufp = NULL;
			return action;
		}
#endif /* DUMMYNET */
		action = pf_normalize_tcp(dir, kif, pbuf, 0, off, h, &pd);
		if (pd.lmw < 0) {
			goto done;
		}
		PF_APPLE_UPDATE_PDESC_IPv6();
		if (action == PF_DROP) {
			goto done;
		}
		if (th.th_sport == 0 || th.th_dport == 0) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_INVPORT);
			goto done;
		}
		action = pf_test_state_tcp(&s, dir, kif, pbuf, off, h, &pd,
		    &reason);
		if (action == PF_NAT64) {
			goto done;
		}
		if (pd.lmw < 0) {
			goto done;
		}
		PF_APPLE_UPDATE_PDESC_IPv6();
		if (action == PF_PASS) {
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL) {
			action = pf_test_rule(&r, &s, dir, kif,
			    pbuf, off, h, &pd, &a, &ruleset, NULL);
		}
		break;
	}

	case IPPROTO_UDP: {
		struct udphdr   uh;

		pf_pd_set_hdr_udp(&pd, &uh);
		if (!pf_pull_hdr(pbuf, off, &uh, sizeof(uh), sizeof(uh),
		    &action, &reason, AF_INET6)) {
			log = action != PF_PASS;
			goto done;
		}
		if (uh.uh_sport == 0 || uh.uh_dport == 0) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_INVPORT);
			goto done;
		}
		if (ntohs(uh.uh_ulen) > pbuf->pb_packet_len - off ||
		    ntohs(uh.uh_ulen) < sizeof(struct udphdr)) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_SHORT);
			goto done;
		}
#if DUMMYNET
		/* Traffic goes through dummynet first */
		action = pf_test_dummynet(&r, dir, kif, &pbuf, &pd, fwa);
		if (action == PF_DROP || pbuf == NULL) {
			*pbufp = NULL;
			return action;
		}
#endif /* DUMMYNET */
		action = pf_test_state_udp(&s, dir, kif, pbuf, off, h, &pd,
		    &reason);
		if (action == PF_NAT64) {
			goto done;
		}
		if (pd.lmw < 0) {
			goto done;
		}
		PF_APPLE_UPDATE_PDESC_IPv6();
		if (action == PF_PASS) {
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL) {
			action = pf_test_rule(&r, &s, dir, kif,
			    pbuf, off, h, &pd, &a, &ruleset, NULL);
		}
		break;
	}

	case IPPROTO_ICMPV6: {
		struct icmp6_hdr        ih;

		pf_pd_set_hdr_icmp6(&pd, &ih);
		if (!pf_pull_hdr(pbuf, off, &ih, sizeof(ih), sizeof(ih),
		    &action, &reason, AF_INET6)) {
			log = action != PF_PASS;
			goto done;
		}
#if DUMMYNET
		/* Traffic goes through dummynet first */
		action = pf_test_dummynet(&r, dir, kif, &pbuf, &pd, fwa);
		if (action == PF_DROP || pbuf == NULL) {
			*pbufp = NULL;
			return action;
		}
#endif /* DUMMYNET */
		action = pf_test_state_icmp(&s, dir, kif,
		    pbuf, off, h, &pd, &reason);
		if (action == PF_NAT64) {
			goto done;
		}
		if (pd.lmw < 0) {
			goto done;
		}
		PF_APPLE_UPDATE_PDESC_IPv6();
		if (action == PF_PASS) {
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL) {
			action = pf_test_rule(&r, &s, dir, kif,
			    pbuf, off, h, &pd, &a, &ruleset, NULL);
		}
		break;
	}

	case IPPROTO_ESP: {
		struct pf_esp_hdr       esp;

		pf_pd_set_hdr_esp(&pd, &esp);
		if (!pf_pull_hdr(pbuf, off, &esp, sizeof(esp), sizeof(esp), &action,
		    &reason, AF_INET6)) {
			log = action != PF_PASS;
			goto done;
		}
#if DUMMYNET
		/* Traffic goes through dummynet first */
		action = pf_test_dummynet(&r, dir, kif, &pbuf, &pd, fwa);
		if (action == PF_DROP || pbuf == NULL) {
			*pbufp = NULL;
			return action;
		}
#endif /* DUMMYNET */
		action = pf_test_state_esp(&s, dir, kif, off, &pd);
		if (pd.lmw < 0) {
			goto done;
		}
		PF_APPLE_UPDATE_PDESC_IPv6();
		if (action == PF_PASS) {
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL) {
			action = pf_test_rule(&r, &s, dir, kif,
			    pbuf, off, h, &pd, &a, &ruleset, NULL);
		}
		break;
	}

	case IPPROTO_GRE: {
		struct pf_grev1_hdr     grev1;

		pf_pd_set_hdr_grev1(&pd, &grev1);
		if (!pf_pull_hdr(pbuf, off, &grev1, sizeof(grev1), sizeof(grev1), &action,
		    &reason, AF_INET6)) {
			log = (action != PF_PASS);
			goto done;
		}
#if DUMMYNET
		/* Traffic goes through dummynet first */
		action = pf_test_dummynet(&r, dir, kif, &pbuf, &pd, fwa);
		if (action == PF_DROP || pbuf == NULL) {
			*pbufp = NULL;
			return action;
		}
#endif /* DUMMYNET */
		if ((ntohs(grev1.flags) & PF_GRE_FLAG_VERSION_MASK) == 1 &&
		    ntohs(grev1.protocol_type) == PF_GRE_PPP_ETHERTYPE) {
			if (ntohs(grev1.payload_length) >
			    pbuf->pb_packet_len - off) {
				action = PF_DROP;
				REASON_SET(&reason, PFRES_SHORT);
				goto done;
			}
			action = pf_test_state_grev1(&s, dir, kif, off, &pd);
			if (pd.lmw < 0) {
				goto done;
			}
			PF_APPLE_UPDATE_PDESC_IPv6();
			if (action == PF_PASS) {
#if NPFSYNC
				pfsync_update_state(s);
#endif /* NPFSYNC */
				r = s->rule.ptr;
				a = s->anchor.ptr;
				log = s->log;
				break;
			} else if (s == NULL) {
				action = pf_test_rule(&r, &s, dir, kif, pbuf,
				    off, h, &pd, &a, &ruleset, NULL);
				if (action == PF_PASS) {
					break;
				}
			}
		}

		/* not GREv1/PPTP, so treat as ordinary GRE... */
		OS_FALLTHROUGH; /* XXX is this correct? */
	}

	default:
#if DUMMYNET
		/* Traffic goes through dummynet first */
		action = pf_test_dummynet(&r, dir, kif, &pbuf, &pd, fwa);
		if (action == PF_DROP || pbuf == NULL) {
			*pbufp = NULL;
			return action;
		}
#endif /* DUMMYNET */
		action = pf_test_state_other(&s, dir, kif, &pd);
		if (pd.lmw < 0) {
			goto done;
		}
		PF_APPLE_UPDATE_PDESC_IPv6();
		if (action == PF_PASS) {
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL) {
			action = pf_test_rule(&r, &s, dir, kif, pbuf, off, h,
			    &pd, &a, &ruleset, NULL);
		}
		break;
	}

done:
	if (action == PF_NAT64) {
		*pbufp = NULL;
		return action;
	}

	*pbufp = pd.mp;
	PF_APPLE_UPDATE_PDESC_IPv6();

	/* handle dangerous IPv6 extension headers. */
	if (action != PF_DROP) {
		if (action == PF_PASS && rh_cnt &&
		    !((s && s->allow_opts) || r->allow_opts)) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_IPOPTIONS);
			log = 1;
			DPFPRINTF(PF_DEBUG_MISC,
			    ("pf: dropping packet with dangerous v6addr headers\n"));
		}

		if ((s && s->tag) || PF_RTABLEID_IS_VALID(r->rtableid) ||
		    (pd.pktflags & PKTF_FLOW_ID)) {
			(void) pf_tag_packet(pbuf, pd.pf_mtag, s ? s->tag : 0,
			    r->rtableid, &pd);
		}

		if (action == PF_PASS) {
#if PF_ECN
			/* add hints for ecn */
			pd.pf_mtag->pftag_hdr = h;
			/* record address family */
			pd.pf_mtag->pftag_flags &= ~PF_TAG_HDR_INET;
			pd.pf_mtag->pftag_flags |= PF_TAG_HDR_INET6;
#endif /* PF_ECN */
			/* record protocol */
			*pbuf->pb_proto = pd.proto;
			if (dir == PF_IN && (pd.proto == IPPROTO_TCP ||
			    pd.proto == IPPROTO_UDP) && s != NULL &&
			    s->nat_rule.ptr != NULL &&
			    (s->nat_rule.ptr->action == PF_RDR ||
			    s->nat_rule.ptr->action == PF_BINAT) &&
			    IN6_IS_ADDR_LOOPBACK(&pd.dst->v6addr)) {
				pd.pf_mtag->pftag_flags |= PF_TAG_TRANSLATE_LOCALHOST;
			}
		}
	}


	if (log) {
		struct pf_rule *lr;

		if (s != NULL && s->nat_rule.ptr != NULL &&
		    s->nat_rule.ptr->log & PF_LOG_ALL) {
			lr = s->nat_rule.ptr;
		} else {
			lr = r;
		}
		PFLOG_PACKET(kif, h, pbuf, AF_INET6, dir, reason, lr, a, ruleset,
		    &pd);
	}

	kif->pfik_bytes[1][dir == PF_OUT][action != PF_PASS] += pd.tot_len;
	kif->pfik_packets[1][dir == PF_OUT][action != PF_PASS]++;

	if (action == PF_PASS || r->action == PF_DROP) {
		dirndx = (dir == PF_OUT);
		r->packets[dirndx]++;
		r->bytes[dirndx] += pd.tot_len;
		if (a != NULL) {
			a->packets[dirndx]++;
			a->bytes[dirndx] += pd.tot_len;
		}
		if (s != NULL) {
			sk = s->state_key;
			if (s->nat_rule.ptr != NULL) {
				s->nat_rule.ptr->packets[dirndx]++;
				s->nat_rule.ptr->bytes[dirndx] += pd.tot_len;
			}
			if (s->src_node != NULL) {
				s->src_node->packets[dirndx]++;
				s->src_node->bytes[dirndx] += pd.tot_len;
			}
			if (s->nat_src_node != NULL) {
				s->nat_src_node->packets[dirndx]++;
				s->nat_src_node->bytes[dirndx] += pd.tot_len;
			}
			dirndx = (dir == sk->direction) ? 0 : 1;
			s->packets[dirndx]++;
			s->bytes[dirndx] += pd.tot_len;
		}
		tr = r;
		nr = (s != NULL) ? s->nat_rule.ptr : pd.nat_rule;
		if (nr != NULL) {
			struct pf_addr *x;
			/*
			 * XXX: we need to make sure that the addresses
			 * passed to pfr_update_stats() are the same than
			 * the addresses used during matching (pfr_match)
			 */
			if (r == &pf_default_rule) {
				tr = nr;
				x = (s == NULL || sk->direction == dir) ?
				    &pd.baddr : &pd.naddr;
			} else {
				x = (s == NULL || sk->direction == dir) ?
				    &pd.naddr : &pd.baddr;
			}
			if (x == &pd.baddr || s == NULL) {
				if (dir == PF_OUT) {
					pd.src = x;
				} else {
					pd.dst = x;
				}
			}
		}
		if (tr->src.addr.type == PF_ADDR_TABLE) {
			pfr_update_stats(tr->src.addr.p.tbl, (sk == NULL ||
			    sk->direction == dir) ? pd.src : pd.dst, pd.af,
			    pd.tot_len, dir == PF_OUT, r->action == PF_PASS,
			    tr->src.neg);
		}
		if (tr->dst.addr.type == PF_ADDR_TABLE) {
			pfr_update_stats(tr->dst.addr.p.tbl, (sk == NULL ||
			    sk->direction == dir) ? pd.dst : pd.src, pd.af,
			    pd.tot_len, dir == PF_OUT, r->action == PF_PASS,
			    tr->dst.neg);
		}
	}

	VERIFY(pbuf == NULL || pd.mp == NULL || pd.mp == pbuf);

	if (*pbufp) {
		if (pd.lmw < 0) {
			REASON_SET(&reason, PFRES_MEMORY);
			action = PF_DROP;
		}

		if (action == PF_DROP) {
			pbuf_destroy(*pbufp);
			*pbufp = NULL;
			return PF_DROP;
		}

		*pbufp = pbuf;
	}

	if (action == PF_SYNPROXY_DROP) {
		pbuf_destroy(*pbufp);
		*pbufp = NULL;
		action = PF_PASS;
	} else if (r->rt) {
		/* pf_route6 can free the mbuf causing *pbufp to become NULL */
		pf_route6(pbufp, r, dir, kif->pfik_ifp, s, &pd);
	}

	/* if reassembled packet passed, create new fragments */
	struct pf_fragment_tag *ftag = NULL;
	if ((action == PF_PASS) && (*pbufp != NULL) && (fwd) &&
	    ((ftag = pf_find_fragment_tag_pbuf(*pbufp)) != NULL)) {
		action = pf_refragment6(ifp, pbufp, ftag);
	}
	return action;
}

static int
pf_check_congestion(struct ifqueue *ifq)
{
#pragma unused(ifq)
	return 0;
}

void
pool_init(struct pool *pp, size_t size, unsigned int align, unsigned int ioff,
    int flags, const char *wchan, void *palloc)
{
#pragma unused(align, ioff, flags, palloc)
	bzero(pp, sizeof(*pp));
	pp->pool_zone = zone_create(wchan, size,
	    ZC_PGZ_USE_GUARDS | ZC_ZFREE_CLEARMEM);
	pp->pool_hiwat = pp->pool_limit = (unsigned int)-1;
	pp->pool_name = wchan;
}

/* Zones cannot be currently destroyed */
void
pool_destroy(struct pool *pp)
{
#pragma unused(pp)
}

void
pool_sethiwat(struct pool *pp, int n)
{
	pp->pool_hiwat = n;     /* Currently unused */
}

void
pool_sethardlimit(struct pool *pp, int n, const char *warnmess, int ratecap)
{
#pragma unused(warnmess, ratecap)
	pp->pool_limit = n;
}

void *
pool_get(struct pool *pp, int flags)
{
	void *buf;

	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	if (pp->pool_count > pp->pool_limit) {
		DPFPRINTF(PF_DEBUG_NOISY,
		    ("pf: pool %s hard limit reached (%d)\n",
		    pp->pool_name != NULL ? pp->pool_name : "unknown",
		    pp->pool_limit));
		pp->pool_fails++;
		return NULL;
	}

	buf = zalloc_flags_buf(pp->pool_zone,
	    (flags & PR_WAITOK) ? Z_WAITOK : Z_NOWAIT);
	if (buf != NULL) {
		pp->pool_count++;
		VERIFY(pp->pool_count != 0);
	}
	return buf;
}

void
pool_put(struct pool *pp, void *v)
{
	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);

	zfree(pp->pool_zone, v);
	VERIFY(pp->pool_count != 0);
	pp->pool_count--;
}

struct pf_mtag *
pf_find_mtag_pbuf(pbuf_t *pbuf)
{
	return pbuf->pb_pftag;
}

struct pf_mtag *
pf_find_mtag(struct mbuf *m)
{
	return m_pftag(m);
}

struct pf_mtag *
pf_get_mtag(struct mbuf *m)
{
	return pf_find_mtag(m);
}

struct pf_mtag *
pf_get_mtag_pbuf(pbuf_t *pbuf)
{
	return pf_find_mtag_pbuf(pbuf);
}

struct pf_fragment_tag *
pf_copy_fragment_tag(struct mbuf *m, struct pf_fragment_tag *ftag, int how)
{
	struct m_tag *__single tag;
	struct pf_mtag *__single pftag = pf_find_mtag(m);

	tag = m_tag_create(KERNEL_MODULE_TAG_ID, KERNEL_TAG_TYPE_PF_REASS,
	    sizeof(*ftag), how, m);
	if (tag == NULL) {
		return NULL;
	}
	m_tag_prepend(m, tag);
	bcopy(ftag, tag->m_tag_data, sizeof(*ftag));
	pftag->pftag_flags |= PF_TAG_REASSEMBLED;
	return (struct pf_fragment_tag *)tag->m_tag_data;
}

struct pf_fragment_tag *
pf_find_fragment_tag(struct mbuf *m)
{
	struct m_tag *tag;
	struct pf_fragment_tag *ftag = NULL;
	struct pf_mtag *pftag = pf_find_mtag(m);

	tag = m_tag_locate(m, KERNEL_MODULE_TAG_ID, KERNEL_TAG_TYPE_PF_REASS);
	VERIFY((tag == NULL) || (pftag->pftag_flags & PF_TAG_REASSEMBLED));
	if (tag != NULL) {
		ftag = (struct pf_fragment_tag *)tag->m_tag_data;
	}
	return ftag;
}

struct pf_fragment_tag *
pf_find_fragment_tag_pbuf(pbuf_t *pbuf)
{
	struct pf_mtag *mtag = pf_find_mtag_pbuf(pbuf);

	return (mtag->pftag_flags & PF_TAG_REASSEMBLED) ?
	       pbuf->pb_pf_fragtag : NULL;
}

uint64_t
pf_time_second(void)
{
	struct timeval t;

	microuptime(&t);
	return t.tv_sec;
}

uint64_t
pf_calendar_time_second(void)
{
	struct timeval t;

	getmicrotime(&t);
	return t.tv_sec;
}

static void *
hook_establish(struct hook_desc_head *head, int tail, hook_fn_t fn, void *arg)
{
	struct hook_desc *hd;

	hd = kalloc_type(struct hook_desc, Z_WAITOK | Z_NOFAIL);

	hd->hd_fn = fn;
	hd->hd_arg = arg;
	if (tail) {
		TAILQ_INSERT_TAIL(head, hd, hd_list);
	} else {
		TAILQ_INSERT_HEAD(head, hd, hd_list);
	}

	return hd;
}

static void
hook_runloop(struct hook_desc_head *head, int flags)
{
	struct hook_desc *__single hd;

	if (!(flags & HOOK_REMOVE)) {
		if (!(flags & HOOK_ABORT)) {
			TAILQ_FOREACH(hd, head, hd_list)
			hd->hd_fn(hd->hd_arg);
		}
	} else {
		while (!!(hd = TAILQ_FIRST(head))) {
			TAILQ_REMOVE(head, hd, hd_list);
			if (!(flags & HOOK_ABORT)) {
				hd->hd_fn(hd->hd_arg);
			}
			if (flags & HOOK_FREE) {
				kfree_type(struct hook_desc, hd);
			}
		}
	}
}

#if SKYWALK
static uint32_t
pf_check_compatible_anchor(struct pf_anchor const * a)
{
	const char *__null_terminated anchor_path = __unsafe_null_terminated_from_indexable(a->path);
	uint32_t result = 0;

	if (strcmp(anchor_path, PF_RESERVED_ANCHOR) == 0) {
		goto done;
	}

	if (strcmp(anchor_path, "com.apple") == 0) {
		goto done;
	}

	for (int i = 0; i < sizeof(compatible_anchors) / sizeof(compatible_anchors[0]); i++) {
		const char *__null_terminated ptr = strnstr(anchor_path, compatible_anchors[i], MAXPATHLEN);
		if (ptr != NULL && ptr == anchor_path) {
			goto done;
		}
	}

	result |= PF_COMPATIBLE_FLAGS_CUSTOM_ANCHORS_PRESENT;
	for (int i = PF_RULESET_SCRUB; i < PF_RULESET_MAX; ++i) {
		if (a->ruleset.rules[i].active.rcount != 0) {
			result |= PF_COMPATIBLE_FLAGS_CUSTOM_RULES_PRESENT;
		}
	}
done:
	return result;
}

uint32_t
pf_check_compatible_rules(void)
{
	LCK_RW_ASSERT(&pf_perim_lock, LCK_RW_ASSERT_HELD);
	LCK_MTX_ASSERT(&pf_lock, LCK_MTX_ASSERT_OWNED);
	struct pf_anchor *anchor = NULL;
	struct pf_rule *rule = NULL;
	uint32_t compat_bitmap = 0;

	if (PF_IS_ENABLED) {
		compat_bitmap |= PF_COMPATIBLE_FLAGS_PF_ENABLED;
	}

	RB_FOREACH(anchor, pf_anchor_global, &pf_anchors) {
		compat_bitmap |= pf_check_compatible_anchor(anchor);
#define _CHECK_FLAGS    (PF_COMPATIBLE_FLAGS_CUSTOM_ANCHORS_PRESENT | PF_COMPATIBLE_FLAGS_CUSTOM_RULES_PRESENT)
		if ((compat_bitmap & _CHECK_FLAGS) == _CHECK_FLAGS) {
			goto done;
		}
#undef _CHECK_FLAGS
	}

	for (int i = PF_RULESET_SCRUB; i < PF_RULESET_MAX; i++) {
		TAILQ_FOREACH(rule, pf_main_ruleset.rules[i].active.ptr, entries) {
			if (rule->anchor == NULL) {
				compat_bitmap |= PF_COMPATIBLE_FLAGS_CUSTOM_RULES_PRESENT;
				goto done;
			}
		}
	}

done:
	return compat_bitmap;
}
#endif // SKYWALK
