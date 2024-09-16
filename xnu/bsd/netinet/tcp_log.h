/*
 * Copyright (c) 2018-2023 Apple Inc. All rights reserved.
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


#ifndef _NETINET_TCP_LOG_H_
#define _NETINET_TCP_LOG_H_

#ifdef PRIVATE

#define TCP_ENABLE_FLAG_LIST \
	X(TLEF_CONNECTION,	0x00000001, connection) \
	X(TLEF_RTT,		0x00000002, rtt)        \
	X(TLEF_KEEP_ALIVE,	0x00000004, ka)         \
	X(TLEF_LOG,		0x00000008, log)        \
	X(TLEF_DST_LOOPBACK,	0x00000010, loop)       \
	X(TLEF_DST_LOCAL,	0x00000020, local)      \
	X(TLEF_DST_GW,		0x00000040, gw)         \
	X(TLEF_THF_SYN,		0x00000100, syn)        \
	X(TLEF_THF_FIN,		0x00000200, fin)        \
	X(TLEF_THF_RST,		0x00000400, rst)        \
	X(TLEF_DROP_NECP,	0x00001000, dropnecp)   \
	X(TLEF_DROP_PCB,	0x00002000, droppcb)    \
	X(TLEF_DROP_PKT,	0x00004000, droppkt)    \
	X(TLEF_FSW_FLOW,	0x00008000, fswflow)    \
	X(TLEF_STATE,           0x00010000, state)      \
	X(TLEF_SYN_RXMT,	0x00020000, synrxmt)    \
	X(TLEF_OUTPUT,	        0x00040000, output)

/*
 * Flag values for tcp_log_enabled
 */
enum {
#define X(name, value, ...) name = value,
	TCP_ENABLE_FLAG_LIST
#undef X
};

#endif /* PRIVATE */

#ifdef BSD_KERNEL_PRIVATE

#include <sys/param.h>
#include <sys/systm.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_pcb.h>
#include <netinet6/in6_pcb.h>

#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet6/tcp6_var.h>

#include <netinet/inp_log.h>

#include <net/net_log_common.h>

#include <os/log.h>

#include <stdbool.h>

extern os_log_t tcp_mpkl_log_object;
extern uint32_t tcp_log_enable_flags;
extern uint16_t tcp_log_port;

#define TLEF_MASK_DST (TLEF_DST_LOOPBACK | TLEF_DST_LOCAL | TLEF_DST_GW)

extern void tcp_log_connection_summary(struct tcpcb *tp);
extern void tcp_log_th_flags(void *hdr, struct tcphdr *th, struct tcpcb *tp, bool outgoing, struct ifnet *ifp);
extern void tcp_log_connection(struct tcpcb *tp, const char *event, int error);
extern void tcp_log_listen(struct tcpcb *tp, int error);
extern void tcp_log_drop_pcb(void *hdr, struct tcphdr *th, struct tcpcb *tp, bool outgoing, const char *reason);
extern void tcp_log_drop_pkt(void *hdr, struct tcphdr *th, struct ifnet *ifp, const char *reason);
extern void tcp_log_rtt_info(const char *func_name, int line_no, struct tcpcb *tp);
extern void tcp_log_rt_rtt(const char *func_name, int line_no, struct tcpcb *tp, struct rtentry *rt);
extern void tcp_log_rtt_change(const char *func_name, int line_no, struct tcpcb *tp, int old_srtt, int old_rttvar);
extern void tcp_log_keepalive(const char *func_name, int line_no, struct tcpcb *tp, int32_t idle_time);
extern void tcp_log_message(const char *func_name, int line_no, struct tcpcb *tp, const char *format, ...) __printflike(4, 5);
extern void tcp_log_fsw_flow(const char *func_name, int line_no, struct tcpcb *tp, const char *format, ...) __printflike(4, 5);
extern void tcp_log_state_change(struct tcpcb *tp, int new_state);
extern void tcp_log_output(const char *func_name, int line_no, struct tcpcb *tp, const char *format, ...) __printflike(4, 5);

static inline bool
tcp_is_log_enabled(struct tcpcb *tp, uint32_t req_flags)
{
	struct inpcb *inp;

	if (tp == NULL || tp->t_inpcb == NULL) {
		return false;
	}
	inp = tp->t_inpcb;
	if (tcp_log_port > 0 &&
	    ntohs(inp->inp_lport) != tcp_log_port &&
	    ntohs(tp->t_inpcb->inp_fport) != tcp_log_port) {
		return false;
	}

	/*
	 * First find out the kind of destination
	 */
	if (inp->inp_log_flags == 0) {
		if (tp->t_inpcb->inp_vflag & INP_IPV6) {
			if (IN6_IS_ADDR_LOOPBACK(&tp->t_inpcb->in6p_laddr) ||
			    IN6_IS_ADDR_LOOPBACK(&tp->t_inpcb->in6p_faddr)) {
				inp->inp_log_flags |= TLEF_DST_LOOPBACK;
			}
		} else {
			if (ntohl(tp->t_inpcb->inp_laddr.s_addr) == INADDR_LOOPBACK ||
			    ntohl(tp->t_inpcb->inp_faddr.s_addr) == INADDR_LOOPBACK) {
				inp->inp_log_flags |= TLEF_DST_LOOPBACK;
			}
		}
		if (inp->inp_log_flags == 0) {
			if (tp->t_flags & TF_LOCAL) {
				inp->inp_log_flags |= TLEF_DST_LOCAL;
			} else {
				inp->inp_log_flags |= TLEF_DST_GW;
			}
		}
	}
	/*
	 * Check separately the destination flags that are per TCP connection
	 * and the other functional flags that are global
	 */
	return (inp->inp_log_flags & tcp_log_enable_flags & TLEF_MASK_DST) &&
	       (tcp_log_enable_flags & (req_flags & ~TLEF_MASK_DST));
}

#define TCP_LOG_RTT_INFO(tp) if (tcp_is_log_enabled(tp, TLEF_RTT)) \
    tcp_log_rtt_info(__func__, __LINE__, (tp))

#define TCP_LOG_RTM_RTT(tp, rt) if (tcp_is_log_enabled(tp, TLEF_RTT)) \
    tcp_log_rt_rtt(__func__, __LINE__, (tp), (rt))

#define TCP_LOG_RTT_CHANGE(tp, old_srtt, old_rttvar) if (tcp_is_log_enabled(tp, TLEF_RTT)) \
    tcp_log_rtt_change(__func__, __LINE__, (tp), (old_srtt), (old_rttvar))

#define TCP_LOG_KEEP_ALIVE(tp, idle_time) if (tcp_is_log_enabled(tp, TLEF_KEEP_ALIVE)) \
    tcp_log_keepalive(__func__, __LINE__, (tp), (idle_time))

#define TCP_LOG_CONNECT(tp, outgoing, error) if (tcp_is_log_enabled(tp, TLEF_CONNECTION)) \
    tcp_log_connection((tp), __unsafe_forge_null_terminated(const char *, ((outgoing) ? "connect outgoing" : "connect incoming")), (error))

#define TCP_LOG_CONNECTED(tp, error) if (tcp_is_log_enabled(tp, TLEF_CONNECTION)) \
    tcp_log_connection((tp), "connected", (error))

#define TCP_LOG_LISTEN(tp, error) if (tcp_is_log_enabled(tp, TLEF_CONNECTION)) \
    tcp_log_listen((tp), (error))

#define TCP_LOG_ACCEPT(tp, error) if (tcp_is_log_enabled(tp, TLEF_CONNECTION)) \
    tcp_log_connection((tp), "accept", (error))

#define TCP_LOG_CONNECTION_SUMMARY(tp) if (tcp_is_log_enabled(tp, TLEF_CONNECTION)) \
    tcp_log_connection_summary((tp))

#define TCP_LOG_DROP_NECP(hdr, th, tp, outgoing) if (tcp_is_log_enabled(tp, TLEF_DROP_NECP)) \
    tcp_log_drop_pcb((hdr), (th), (tp), (outgoing), "NECP")

#define TCP_LOG_DROP_PCB(hdr, th, tp, outgoing, reason) if (tcp_is_log_enabled(tp, TLEF_DROP_PCB)) \
    tcp_log_drop_pcb((hdr), (th), (tp), (outgoing), reason)

#define TCP_LOG_TH_FLAGS(hdr, th, tp, outgoing, ifp) \
    if ((th) != NULL && ((th)->th_flags & (TH_SYN|TH_FIN|TH_RST))) \
	    tcp_log_th_flags((hdr), (th), (tp), (outgoing), (ifp))

#define TCP_LOG_DROP_PKT(hdr, th, ifp, reason) \
    if ((th) != NULL && ((th->th_flags) & (TH_SYN|TH_FIN|TH_RST)) && \
	(tcp_log_enable_flags & TLEF_DROP_PKT)) \
	        tcp_log_drop_pkt((hdr), (th), (ifp), (reason))

#define TCP_LOG_FSW_FLOW(tp, format, ...) if (tcp_is_log_enabled(tp, TLEF_FSW_FLOW)) \
    tcp_log_fsw_flow(__func__, __LINE__, (tp), format, ##__VA_ARGS__)

#define TCP_LOG(tp, format, ...) if (tcp_is_log_enabled(tp, TLEF_LOG)) \
    tcp_log_message(__func__, __LINE__, tp, format, ## __VA_ARGS__)

#define TCP_LOG_STATE(tp, new_state) if (tcp_is_log_enabled(tp, TLEF_STATE)) \
    tcp_log_state_change((tp), (new_state))

#define TCP_LOG_OUTPUT(tp, format, ...) if (tcp_is_log_enabled(tp, TLEF_OUTPUT)) \
    tcp_log_output(__func__, __LINE__, tp, format, ## __VA_ARGS__)

#endif /* BSD_KERNEL_PRIVATE */

#endif /* _NETINET_TCP_LOG_H_ */
