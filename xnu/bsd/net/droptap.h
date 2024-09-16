/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#ifndef _NET_DROPTAP_H_
#define _NET_DROPTAP_H_

#ifdef PRIVATE
#include <net/pktap.h>

#define DROPTAP_IFNAME "droptap"
#define DROPTAP_IFXNAMESIZE (IF_NAMESIZE + 8)

#define DROPTAP_DROPFUNC_MAXLEN 64

/*
 * Droptap header is a special type of pktap header with droptap-specific
 * metadata.
 */
struct droptap_header {
	struct pktap_header     dth_pktap_hdr;
	uint32_t                dth_dropreason;
	uint8_t                 dth_dropfunc_size;
	uint16_t                dth_dropline;
	char                    dth_dropfunc[DROPTAP_DROPFUNC_MAXLEN];
};

/*
 * If we do not want to store function name and line number, client should pass
 * NULL to dropfunc, and then droptap internally sets dth_dropfunc_size to 0 and
 * copies fields up to dth_dropfunc_size to bpf.
 */
#define DROPTAP_HDR_SIZE(dtaphdr)                                                                           \
	(((dtaphdr)->dth_dropfunc_size == 0) ?                                                              \
	    (__offsetof(struct droptap_header, dth_dropfunc_size) + sizeof((dtaphdr)->dth_dropfunc_size)) : \
	    (__offsetof(struct droptap_header, dth_dropfunc) + (dtaphdr)->dth_dropfunc_size + 1))

/*
 * Drop Reason is a 32-bit encoding of drop code, domain, and component.
 *
 *  Reserved  Component     Domain       Drop Code
 * ╭─────────┬─────────┬─────────────┬───────────────╮
 * │    4    │    4    │      8      │       16      │
 * ╰─────────┴─────────┴─────────────┴───────────────╯
 * ╰─────────────────────────────────────────────────╯
 *                   Drop Reason
 *
 * [ 15:0] Drop Code: Specific reason why the drop happened (e.g. AQM full)
 * [23:16] Domain   : Which domain the drop happened (e.g. Flowswitch, TCP, IP)
 * [27:24] Component: Which component the drop happened (e.g. Skywalk, BSD, driver)
 * [31:28] Reserved : Reserved for future use
 *
 */
#define DROP_COMPONENT_MASK   0x0f000000
#define DROP_COMPONENT_OFFSET 24
#define DROP_COMPONENT_MAX    0x0f
#define DROP_DOMAIN_MASK      0x00ff0000
#define DROP_DOMAIN_OFFSET    16
#define DROP_DOMAIN_MAX       0xff
#define DROP_CODE_MASK        0x0000ffff
#define DROP_CODE_OFFSET      0
#define DROP_CODE_MAX         0xffff

/* 32-bit Drop Reason */
#define DROP_REASON(component, domain, code) \
	(((unsigned)((component) &   0x0f) << DROP_COMPONENT_OFFSET) | \
	 ((unsigned)((domain)    &   0xff) << DROP_DOMAIN_OFFSET)    | \
	 ((unsigned)((code)      & 0xffff) << DROP_CODE_OFFSET))

/* All components */
#define DROPTAP_SKYWALK 1
#define DROPTAP_BSD     2

/* All domains for Skywalk component */
#define DROPTAP_FSW     1
#define DROPTAP_NETIF   2

/* All domains for BSD component */
#define DROPTAP_TCP     1
#define DROPTAP_UDP     2
#define DROPTAP_IP      3
#define DROPTAP_SOCK    4
#define DROPTAP_AQM     5
#define DROPTAP_IPSEC   6
#define DROPTAP_IP6     7

#define DROPTAP_UNSPEC  0

#define DROP_REASON_LIST                                                                                                         \
	X(DROP_REASON_UNSPECIFIED,                  DROPTAP_UNSPEC,  DROPTAP_UNSPEC, DROPTAP_UNSPEC, "Drop reason not specified")    \
	/* Skywalk component */                                                                                                      \
	X(DROP_REASON_FSW_PP_ALLOC_FAILED,          DROPTAP_SKYWALK, DROPTAP_FSW,  1,  "Flowswitch packet alloc failed")             \
	X(DROP_REASON_RX_DST_RING_FULL,             DROPTAP_SKYWALK, DROPTAP_FSW,  2,  "Flowswitch Rx destination ring full")        \
	X(DROP_REASON_FSW_QUIESCED,                 DROPTAP_SKYWALK, DROPTAP_FSW,  3,  "Flowswitch detached")                        \
	X(DROP_REASON_FSW_IFNET_NOT_ATTACHED,       DROPTAP_SKYWALK, DROPTAP_FSW,  4,  "Flowswitch ifnet not attached")              \
	X(DROP_REASON_FSW_DEMUX_FAILED,             DROPTAP_SKYWALK, DROPTAP_FSW,  5,  "Flowswitch demux error")                     \
	X(DROP_REASON_FSW_TX_DEVPORT_NOT_ATTACHED,  DROPTAP_SKYWALK, DROPTAP_FSW,  6,  "Flowswitch destination nexus port inactive") \
	X(DROP_REASON_FSW_TX_FLOW_EXTRACT_FAILED,   DROPTAP_SKYWALK, DROPTAP_FSW,  7,  "Flowswitch flow extract error")              \
	X(DROP_REASON_FSW_TX_FRAG_BAD_CONT,         DROPTAP_SKYWALK, DROPTAP_FSW,  8,  "Flowswitch invalid continuation fragment")   \
	X(DROP_REASON_FSW_TX_FLOW_NOT_FOUND,        DROPTAP_SKYWALK, DROPTAP_FSW,  9,  "Flowswitch flow lookup failed")              \
	X(DROP_REASON_FSW_TX_RESOLV_PENDING,        DROPTAP_SKYWALK, DROPTAP_FSW,  10, "Flowswitch resolution pending")              \
	X(DROP_REASON_FSW_TX_RESOLV_FAILED,         DROPTAP_SKYWALK, DROPTAP_FSW,  11, "Flowswitch resolution failed")               \
	X(DROP_REASON_FSW_FLOW_NONVIABLE,           DROPTAP_SKYWALK, DROPTAP_FSW,  12, "Flowswitch flow not viable")                 \
	X(DROP_REASON_FSW_RX_RING_NOT_FOUND,        DROPTAP_SKYWALK, DROPTAP_FSW,  13, "Flowswitch Rx ring not found")               \
	X(DROP_REASON_FSW_RX_PKT_NOT_FINALIZED,     DROPTAP_SKYWALK, DROPTAP_FSW,  14, "Flowswitch packet not finalized")            \
	X(DROP_REASON_FSW_FLOW_TRACK_ERR,           DROPTAP_SKYWALK, DROPTAP_FSW,  15, "Flowswitch flow tracker error")              \
	X(DROP_REASON_FSW_PKT_COPY_FAILED,          DROPTAP_SKYWALK, DROPTAP_FSW,  16, "Flowswitch packet copy failed")              \
	X(DROP_REASON_FSW_GSO_FAILED,               DROPTAP_SKYWALK, DROPTAP_FSW,  17, "Flowswitch GSO failed")                      \
	X(DROP_REASON_FSW_GSO_NOMEM_PKT,            DROPTAP_SKYWALK, DROPTAP_FSW,  18, "Flowswitch GSO not enough packet memory")    \
	X(DROP_REASON_FSW_GSO_NOMEM_MBUF,           DROPTAP_SKYWALK, DROPTAP_FSW,  19, "Flowswitch GSO not enough mbuf memory")      \
	X(DROP_REASON_FSW_DST_NXPORT_INVALID,       DROPTAP_SKYWALK, DROPTAP_FSW,  20, "Flowswitch dst nexus port invalid")          \
	X(DROP_REASON_AQM_FULL,                     DROPTAP_SKYWALK, DROPTAP_AQM,  1,  "AQM full")                                   \
	/* Socket */                                                                                                                 \
	X(DROP_REASON_FULL_SOCK_RCVBUF,             DROPTAP_BSD,     DROPTAP_SOCK, 1,  "Socket receive buffer full")                 \
	/* TCP */                                                                                                                    \
	X(DROP_REASON_TCP_RST,                      DROPTAP_BSD,     DROPTAP_TCP,  1,  "TCP connection reset")                       \
	/* IP */                                                                                                                     \
	X(DROP_REASON_IP_UNKNOWN_MULTICAST_GROUP,   DROPTAP_BSD,     DROPTAP_IP,   2, "IP unknown multicast group join")             \
	X(DROP_REASON_IP_INVALID_ADDR,              DROPTAP_BSD,     DROPTAP_IP,   3, "Invalid IP address")                          \
	X(DROP_REASON_IP_TOO_SHORT,                 DROPTAP_BSD,     DROPTAP_IP,   4, "IP packet too short")                         \
	X(DROP_REASON_IP_TOO_SMALL,                 DROPTAP_BSD,     DROPTAP_IP,   5, "IP header too small")                         \
	X(DROP_REASON_IP_RCV_IF_NO_MATCH,           DROPTAP_BSD,     DROPTAP_IP,   6, "IP receive interface no match")               \
	X(DROP_REASON_IP_CANNOT_FORWARD,            DROPTAP_BSD,     DROPTAP_IP,   7, "IP cannot forward")                           \
	X(DROP_REASON_IP_BAD_VERSION,               DROPTAP_BSD,     DROPTAP_IP,   8, "IP bad version")                              \
	X(DROP_REASON_IP_BAD_CHECKSUM,              DROPTAP_BSD,     DROPTAP_IP,   9, "IP bad checksum")                             \
	X(DROP_REASON_IP_BAD_HDR_LENGTH,            DROPTAP_BSD,     DROPTAP_IP,   10, "IP bad header length")                       \
	X(DROP_REASON_IP_BAD_LENGTH,                DROPTAP_BSD,     DROPTAP_IP,   11, "IP bad length")                              \
	X(DROP_REASON_IP_BAD_TTL,                   DROPTAP_BSD,     DROPTAP_IP,   12, "IP bad TTL")                                 \
	X(DROP_REASON_IP_NO_PROTO,                  DROPTAP_BSD,     DROPTAP_IP,   13, "IP unknown protocol")                        \
	X(DROP_REASON_IP_FRAG_NOT_ACCEPTED,         DROPTAP_BSD,     DROPTAP_IP,   14, "IP fragment not accepted")                   \
	X(DROP_REASON_IP_FRAG_DROPPED,              DROPTAP_BSD,     DROPTAP_IP,   15, "IP fragment dropped")                        \
	X(DROP_REASON_IP_FRAG_TIMEOUT,              DROPTAP_BSD,     DROPTAP_IP,   16, "IP fragment timeout")                        \
	X(DROP_REASON_IP_FRAG_TOO_MANY,             DROPTAP_BSD,     DROPTAP_IP,   17, "IP fragment too many")                       \
	X(DROP_REASON_IP_FRAG_TOO_LONG,             DROPTAP_BSD,     DROPTAP_IP,   18, "IP fragment too long")                       \
	X(DROP_REASON_IP_FRAG_DRAINED,              DROPTAP_BSD,     DROPTAP_IP,   19, "IP fragment drained")                        \
	X(DROP_REASON_IP_FILTER_DROP,               DROPTAP_BSD,     DROPTAP_IP,   20, "IP filter drop")                             \
	X(DROP_REASON_IP_FRAG_TOO_SMALL,            DROPTAP_BSD,     DROPTAP_IP,   21, "IP too small to fragment")                   \
	X(DROP_REASON_IP_FRAG_NO_MEM,               DROPTAP_BSD,     DROPTAP_IP,   22, "IP no memory for fragmentation")             \
	X(DROP_REASON_IP_CANNOT_FRAGMENT,           DROPTAP_BSD,     DROPTAP_IP,   23, "IP cannot fragment")                         \
	X(DROP_REASON_IP_OUTBOUND_IPSEC_POLICY,     DROPTAP_BSD,     DROPTAP_IP,   24, "IP outbound IPsec policy")                   \
	X(DROP_REASON_IP_ZERO_NET,                  DROPTAP_BSD,     DROPTAP_IP,   25, "IP to network zero")                         \
	X(DROP_REASON_IP_SRC_ADDR_NO_AVAIL,         DROPTAP_BSD,     DROPTAP_IP,   26, "IP source address not available")            \
	X(DROP_REASON_IP_DST_ADDR_NO_AVAIL,         DROPTAP_BSD,     DROPTAP_IP,   27, "IP destination address not available")       \
	X(DROP_REASON_IP_TO_RESTRICTED_IF,          DROPTAP_BSD,     DROPTAP_IP,   28, "IP packet to a restricted interface")        \
	X(DROP_REASON_IP_NO_ROUTE,                  DROPTAP_BSD,     DROPTAP_IP,   29, "IP no route")                                \
	X(DROP_REASON_IP_IF_CANNOT_MULTICAST,       DROPTAP_BSD,     DROPTAP_IP,   30, "IP multicast not supported by interface")    \
	X(DROP_REASON_IP_SRC_ADDR_ANY,              DROPTAP_BSD,     DROPTAP_IP,   31, "IP source address any")                      \
	X(DROP_REASON_IP_IF_CANNOT_BROADCAST,       DROPTAP_BSD,     DROPTAP_IP,   32, "IP broadcast not supported by interface")    \
	X(DROP_REASON_IP_BROADCAST_NOT_ALLOWED,     DROPTAP_BSD,     DROPTAP_IP,   33, "IP broadcast not allowed")                   \
	X(DROP_REASON_IP_BROADCAST_TOO_BIG,         DROPTAP_BSD,     DROPTAP_IP,   34, "IP broadcast too big for MTU")               \
	X(DROP_REASON_IP_FILTER_TSO,                DROPTAP_BSD,     DROPTAP_IP,   35, "TSO packet to IP filter")                    \
	X(DROP_REASON_IP_NECP_POLICY_NO_ALLOW_IF,   DROPTAP_BSD,     DROPTAP_IP,   36, "NECP not allowed on interface")              \
	X(DROP_REASON_IP_NECP_POLICY_DROP,          DROPTAP_BSD,     DROPTAP_IP,   37, "NECP drop")                                  \
	X(DROP_REASON_IP_NECP_POLICY_SOCKET_DIVERT, DROPTAP_BSD,     DROPTAP_IP,   38, "NECP socket divert")                         \
	X(DROP_REASON_IP_NECP_POLICY_TUN_NO_ALLOW_IF, DROPTAP_BSD,   DROPTAP_IP,   39, "NECP tunnel not allowed on interface")       \
	X(DROP_REASON_IP_NECP_POLICY_TUN_REBIND_NO_ALLOW_IF, DROPTAP_BSD,     DROPTAP_IP,   40, "NECP rebind not allowed on interface") \
	X(DROP_REASON_IP_NECP_POLICY_TUN_NO_REBIND_IF, DROPTAP_BSD,  DROPTAP_IP,   41, "NECP rebind not allowed on interface")       \
	X(DROP_REASON_IP_NECP_NO_ALLOW_IF,          DROPTAP_BSD,     DROPTAP_IP,   42, "NECP packet not allowed on interface")       \
	X(DROP_REASON_IP_ENOBUFS,                   DROPTAP_BSD,     DROPTAP_IP,   43, "IP No buffer space available")               \
	X(DROP_REASON_IP_ILLEGAL_PORT,              DROPTAP_BSD,     DROPTAP_IP,   44, "IP Illegal port")                            \
	X(DROP_REASON_IP_UNREACHABLE_PORT,          DROPTAP_BSD,     DROPTAP_IP,   45, "IP Unreachable port")                        \
	X(DROP_REASON_IP_MULTICAST_NO_PORT,         DROPTAP_BSD,     DROPTAP_IP,   46, "IP Multicast no port")                       \
	X(DROP_REASON_IP_EISCONN,                   DROPTAP_BSD,     DROPTAP_IP,   47, "IP Socket is already connected")             \
	X(DROP_REASON_IP_EAFNOSUPPORT,              DROPTAP_BSD,     DROPTAP_IP,   48, "IP Address family not supported by protocol family") \
	X(DROP_REASON_IP_NO_SOCK,                   DROPTAP_BSD,     DROPTAP_IP,   49, "IP No matching sock") \
	/* IPsec */                                                                                                                  \
	X(DROP_REASON_IPSEC_REJECT,                 DROPTAP_BSD,     DROPTAP_IPSEC,1,  "IPsec reject")                               \
	/* IPv6 */                                                                                                                   \
	X(DROP_REASON_IP6_OPT_DISCARD,              DROPTAP_BSD,     DROPTAP_IP6,  1, "IPv6 discard option")                         \
	X(DROP_REASON_IP6_IF_IPV6_DISABLED,         DROPTAP_BSD,     DROPTAP_IP6,  2, "IPv6 is disabled on the interface")           \
	X(DROP_REASON_IP6_BAD_SCOPE,                DROPTAP_BSD,     DROPTAP_IP6,  3, "IPv6 bad scope")                              \
	X(DROP_REASON_IP6_UNPROXIED_NS,             DROPTAP_BSD,     DROPTAP_IP6,  4, "IPv6 unproxied mistargeted Neighbor Solicitation") \
	X(DROP_REASON_IP6_BAD_OPTION,               DROPTAP_BSD,     DROPTAP_IP6,  5, "IPv6 bad option")                             \
	X(DROP_REASON_IP6_TOO_MANY_OPTIONS,         DROPTAP_BSD,     DROPTAP_IP6,  6, "IPv6 too many header options")                \
	X(DROP_REASON_IP6_BAD_PATH_MTU,             DROPTAP_BSD,     DROPTAP_IP6,  7, "IPv6 bad path MTU")                           \
	X(DROP_REASON_IP6_NO_PREFERRED_SRC_ADDR,    DROPTAP_BSD,     DROPTAP_IP6,  8, "IPv6 no preferred source address")            \
	X(DROP_REASON_IP6_BAD_HLIM,                 DROPTAP_BSD,     DROPTAP_IP6,  9, "IPv6 bad HLIM")                               \
	X(DROP_REASON_IP6_BAD_DAD,                  DROPTAP_BSD,     DROPTAP_IP6,  10, "IPv6 bad DAD")                               \
	X(DROP_REASON_IP6_NO_ND6ALT_IF,             DROPTAP_BSD,     DROPTAP_IP6,  11, "IPv6 no ND6ALT interface")                   \
	X(DROP_REASON_IP6_BAD_ND_STATE,             DROPTAP_BSD,     DROPTAP_IP6,  12, "IPv6 Bad ND state")                          \
	X(DROP_REASON_IP6_ONLY,                     DROPTAP_BSD,     DROPTAP_IP6,  13, "IPv6 Only")                                  \
	X(DROP_REASON_IP6_ADDR_UNSPECIFIED,         DROPTAP_BSD,     DROPTAP_IP6,  14, "IPv6 Address is unspecified")


typedef enum drop_reason : uint32_t {
#define X(reason, component, domain, code, ...) \
	reason = DROP_REASON(component, domain, code),
	DROP_REASON_LIST
#undef X
} drop_reason_t;

__attribute__((always_inline))
static inline const char *
drop_reason_str(drop_reason_t value)
{
	switch (value) {
#define X(reason, ...) \
	case (reason): return #reason;
		DROP_REASON_LIST
#undef X
	default:
		return NULL;
	}
	;
}

#ifdef BSD_KERNEL_PRIVATE

#define DROPTAP_FLAG_DIR_IN     0x0001
#define DROPTAP_FLAG_DIR_OUT    0x0002
#define DROPTAP_FLAG_L2_MISSING 0x0004

extern uint32_t droptap_total_tap_count;

extern void droptap_init(void);
#if SKYWALK
#include <skywalk/os_skywalk.h>
extern void droptap_input_packet(kern_packet_t, drop_reason_t, const char *,
    uint16_t, uint16_t, struct ifnet *, pid_t, const char *,
    pid_t, const char *, uint8_t, uint32_t);
extern void droptap_output_packet(kern_packet_t, drop_reason_t, const char *,
    uint16_t, uint16_t, struct ifnet *, pid_t, const char *, pid_t, const char *,
    uint8_t, uint32_t);
typedef void
(*drop_func_t)(kern_packet_t, drop_reason_t, const char *, uint16_t, uint16_t,
    struct ifnet *, pid_t, const char *, pid_t, const char *,
    uint8_t, uint32_t);
#endif /* SKYWALK */
extern void droptap_input_mbuf(struct mbuf *, drop_reason_t, const char *,
    uint16_t, uint16_t, struct ifnet *, char *);
extern void droptap_output_mbuf(struct mbuf *, drop_reason_t, const char *,
    uint16_t, uint16_t, struct ifnet *);


#endif /* BSD_KERNEL_PRIVATE */
#endif /* PRIVATE */
#endif /* _NET_DROPTAP_H */
