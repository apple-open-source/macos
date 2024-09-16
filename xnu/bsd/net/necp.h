/*
 * Copyright (c) 2013-2023 Apple Inc. All rights reserved.
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

#ifndef _NET_NECP_H_
#define _NET_NECP_H_

#include <net/net_kev.h>
#ifdef PRIVATE

#include <netinet/in.h>
#include <netinet/in_private.h>
#include <netinet/in_stat.h>
#include <sys/socket.h>
#include <net/if_private.h>
#include <net/if_var_private.h>

#if SKYWALK
#include <skywalk/os_nexus_private.h>
#else /* !SKYWALK */
typedef uint16_t nexus_port_t;
#endif /* !SKYWALK */

/*
 * Name registered by the NECP
 */
#define NECP_CONTROL_NAME "com.apple.net.necp_control"

#define NECP_TLV_LENGTH_UINT32  1

struct necp_packet_header {
	u_int8_t            packet_type;
	u_int8_t                flags;
	u_int32_t           message_id;
};

typedef struct {
	uid_t               uid;
	uuid_t              effective_uuid;
	uid_t               persona_id;
} necp_application_id_t;

/*
 * Control message commands
 */
#define NECP_PACKET_TYPE_POLICY_ADD                             1
#define NECP_PACKET_TYPE_POLICY_GET                             2
#define NECP_PACKET_TYPE_POLICY_DELETE                  3
#define NECP_PACKET_TYPE_POLICY_APPLY_ALL               4
#define NECP_PACKET_TYPE_POLICY_LIST_ALL                5
#define NECP_PACKET_TYPE_POLICY_DELETE_ALL              6
#define NECP_PACKET_TYPE_SET_SESSION_PRIORITY   7
#define NECP_PACKET_TYPE_LOCK_SESSION_TO_PROC   8
#define NECP_PACKET_TYPE_REGISTER_SERVICE               9
#define NECP_PACKET_TYPE_UNREGISTER_SERVICE             10
#define NECP_PACKET_TYPE_POLICY_DUMP_ALL                11

/*
 * Session actions
 */
#define NECP_SESSION_ACTION_POLICY_ADD                          1       // In: Policy TLVs				Out: necp_policy_id
#define NECP_SESSION_ACTION_POLICY_GET                          2       // In: necp_policy_id			Out: Policy TLVs
#define NECP_SESSION_ACTION_POLICY_DELETE                       3       // In: necp_policy_id			Out: None
#define NECP_SESSION_ACTION_POLICY_APPLY_ALL            4       // In: None						Out: None
#define NECP_SESSION_ACTION_POLICY_LIST_ALL                     5       // In: None						Out: TLVs of IDs
#define NECP_SESSION_ACTION_POLICY_DELETE_ALL           6       // In: None						Out: None
#define NECP_SESSION_ACTION_SET_SESSION_PRIORITY        7       // In: necp_session_priority	Out: None
#define NECP_SESSION_ACTION_LOCK_SESSION_TO_PROC        8       // In: None						Out: None
#define NECP_SESSION_ACTION_REGISTER_SERVICE            9       // In: uuid_t					Out: None
#define NECP_SESSION_ACTION_UNREGISTER_SERVICE          10      // In: uuid_t					Out: None
#define NECP_SESSION_ACTION_POLICY_DUMP_ALL                     11      // In: None						Out: uint32_t bytes length, then Policy TLVs
#define NECP_SESSION_ACTION_ADD_DOMAIN_FILTER           12      // In: struct net_bloom_filter  Out: uint32_t, ID
#define NECP_SESSION_ACTION_REMOVE_DOMAIN_FILTER        13      // In: uint32_t, ID             Out: None
#define NECP_SESSION_ACTION_REMOVE_ALL_DOMAIN_FILTERS   14      // In: None                     Out: None

/*
 * Control message flags
 */
#define NECP_PACKET_FLAGS_RESPONSE                              0x01    // Used for acks, errors, and query responses

/*
 * Control message TLV types
 */
#define NECP_TLV_NIL                                                    0
#define NECP_TLV_ERROR                                                  1       // u_int32_t
#define NECP_TLV_POLICY_ORDER                                   2       // u_int32_t
#define NECP_TLV_POLICY_CONDITION                               3
#define NECP_TLV_POLICY_RESULT                                  4
#define NECP_TLV_POLICY_ID                                              5       // u_int32_t
#define NECP_TLV_SESSION_PRIORITY                               6       // u_int32_t
#define NECP_TLV_ATTRIBUTE_DOMAIN                               7       // char[]
#define NECP_TLV_ATTRIBUTE_ACCOUNT                              8       // char[]
#define NECP_TLV_SERVICE_UUID                                   9       // uuid_t
#define NECP_TLV_ROUTE_RULE                                             10
#define NECP_TLV_ATTRIBUTE_DOMAIN_OWNER                        11       // char[]
#define NECP_TLV_ATTRIBUTE_TRACKER_DOMAIN                      12       // char[]
#define NECP_TLV_ATTRIBUTE_DOMAIN_CONTEXT                      13       // char[]

/*
 * Control message TLV sent only by the kernel to userspace
 */
#define NECP_TLV_POLICY_OWNER                                   100     // char []
#define NECP_TLV_POLICY_DUMP                                    101
#define NECP_TLV_POLICY_RESULT_STRING                   102     // char []
#define NECP_TLV_POLICY_SESSION_ORDER                   103     // u_int32_t

/*
 * Condition flags
 */
#define NECP_POLICY_CONDITION_FLAGS_NEGATIVE    0x01 // Negative
#define NECP_POLICY_CONDITION_FLAGS_EXACT       0x02 // For conditions that would otherwise have more inclusive matching, require an exact match

/*
 * Added support for dumping negative conditions
 */
#define NECP_POLICY_CONDITION_FLAGS_NEGATIVE_SUPPORTS_DUMP    1

/*
 * Conditions
 * Used for setting policies as well as passing parameters to necp_match_policy.
 */
#define NECP_POLICY_CONDITION_DEFAULT                   0       // N/A, not valid with any other conditions
// Socket/Application conditions
#define NECP_POLICY_CONDITION_APPLICATION               1       // uuid_t, uses effective UUID when possible
#define NECP_POLICY_CONDITION_REAL_APPLICATION  2       // uuid_t, never uses effective UUID. Only valid with NECP_POLICY_CONDITION_APPLICATION
#define NECP_POLICY_CONDITION_DOMAIN                    3       // String, such as apple.com
#define NECP_POLICY_CONDITION_ACCOUNT                   4       // String
#define NECP_POLICY_CONDITION_ENTITLEMENT               5       // String
#define NECP_POLICY_CONDITION_PID                               6       // pid_t
#define NECP_POLICY_CONDITION_UID                               7       // uid_t
#define NECP_POLICY_CONDITION_ALL_INTERFACES    8       // N/A
#define NECP_POLICY_CONDITION_BOUND_INTERFACE   9       // String
#define NECP_POLICY_CONDITION_TRAFFIC_CLASS             10      // necp_policy_condition_tc_range
// Socket/IP conditions
#define NECP_POLICY_CONDITION_IP_PROTOCOL               11      // u_int16_t
#define NECP_POLICY_CONDITION_LOCAL_ADDR                12      // necp_policy_condition_addr
#define NECP_POLICY_CONDITION_REMOTE_ADDR               13      // necp_policy_condition_addr
#define NECP_POLICY_CONDITION_LOCAL_ADDR_RANGE  14      // necp_policy_condition_addr_range
#define NECP_POLICY_CONDITION_REMOTE_ADDR_RANGE 15      // necp_policy_condition_addr_range
#define NECP_POLICY_CONDITION_AGENT_TYPE                16      // struct necp_policy_condition_agent_type
#define NECP_POLICY_CONDITION_HAS_CLIENT                17      // N/A
#define NECP_POLICY_CONDITION_LOCAL_NETWORKS            18      // Matches all local networks
// Socket-only conditions
#define NECP_POLICY_CONDITION_FLOW_IP_PROTOCOL          19      // u_int16_t
#define NECP_POLICY_CONDITION_FLOW_LOCAL_ADDR           20      // necp_policy_condition_addr
#define NECP_POLICY_CONDITION_FLOW_REMOTE_ADDR          21      // necp_policy_condition_addr
#define NECP_POLICY_CONDITION_FLOW_LOCAL_ADDR_RANGE     22      // necp_policy_condition_addr_range
#define NECP_POLICY_CONDITION_FLOW_REMOTE_ADDR_RANGE    23      // necp_policy_condition_addr_range
#define NECP_POLICY_CONDITION_FLOW_IS_LOOPBACK          31      // N/A
// Socket/Application conditions, continued
#define NECP_POLICY_CONDITION_CLIENT_FLAGS              24      // u_int32_t, values from NECP_CLIENT_PARAMETER_FLAG_*
#define NECP_POLICY_CONDITION_FLOW_LOCAL_ADDR_EMPTY     25      // N/A
#define NECP_POLICY_CONDITION_FLOW_REMOTE_ADDR_EMPTY    26      // N/A
#define NECP_POLICY_CONDITION_PLATFORM_BINARY           27      // N/A
#define NECP_POLICY_CONDITION_SDK_VERSION               28      // struct necp_policy_condition_sdk_version
#define NECP_POLICY_CONDITION_SIGNING_IDENTIFIER        29      // String
#define NECP_POLICY_CONDITION_PACKET_FILTER_TAGS        30      // u_int16_t
#define NECP_POLICY_CONDITION_DELEGATE_IS_PLATFORM_BINARY      32      // N/A
#define NECP_POLICY_CONDITION_DOMAIN_OWNER              33      // String, owner of domain (for example, "Google Inc")
#define NECP_POLICY_CONDITION_DOMAIN_CONTEXT            34      // String, eTLD+1 leading to networking activities to the domain
#define NECP_POLICY_CONDITION_TRACKER_DOMAIN            35      // String, tracker domain
#define NECP_POLICY_CONDITION_ATTRIBUTED_BUNDLE_IDENTIFIER 36   // String, app to which traffic is attributed to
#define NECP_POLICY_CONDITION_SCHEME_PORT               37      // u_int16_t, the port associated with the scheme for a connection
#define NECP_POLICY_CONDITION_DOMAIN_FILTER             38      // struct net_bloom_filter
#define NECP_POLICY_CONDITION_SYSTEM_SIGNED_RESULT      39      // N/A
#define NECP_POLICY_CONDITION_REAL_UID                  40      // uid_t
#define NECP_POLICY_CONDITION_APPLICATION_ID            41      // necp_application_id_t
#define NECP_POLICY_CONDITION_URL                       42      // String, URL
#define NECP_POLICY_CONDITION_BOUND_INTERFACE_FLAGS     43      // Interface flags: u_int32_t flags, u_int32_t eflags, u_int32_t xflags


/*
 * Policy Condition Bound Interface Flags Order
 */
#define NECP_POLICY_CONDITION_BOUND_INTERFACE_FLAGS_IDX_FLAGS      0
#define NECP_POLICY_CONDITION_BOUND_INTERFACE_FLAGS_IDX_EFLAGS     1
#define NECP_POLICY_CONDITION_BOUND_INTERFACE_FLAGS_IDX_XFLAGS     2
#define NECP_POLICY_CONDITION_BOUND_INTERFACE_FLAGS_IDX_MAX        3

/*
 * Policy Packet tags
 */
#define NECP_POLICY_CONDITION_PACKET_FILTER_TAG_STACK_DROP         0x01
#define NECP_POLICY_CONDITION_PACKET_FILTER_TAG_MAX                NECP_POLICY_CONDITION_PACKET_FILTER_TAG_STACK_DROP

/*
 * Results
 */
#define NECP_POLICY_RESULT_PASS                                 1       // N/A
#define NECP_POLICY_RESULT_SKIP                                 2       // u_int32_t, policy order to skip to. 0 to skip all session policies.
#define NECP_POLICY_RESULT_DROP                                 3       // N/A
#define NECP_POLICY_RESULT_SOCKET_DIVERT                4       // u_int32_t, flow divert control unit
#define NECP_POLICY_RESULT_SOCKET_FILTER                5       // u_int32_t, filter control unit
#define NECP_POLICY_RESULT_IP_TUNNEL                    6       // String, interface name
#define NECP_POLICY_RESULT_IP_FILTER                    7       // ?
#define NECP_POLICY_RESULT_TRIGGER                              8       // Deprecated
#define NECP_POLICY_RESULT_TRIGGER_IF_NEEDED    9       // Deprecated
#define NECP_POLICY_RESULT_TRIGGER_SCOPED               10      // Deprecated
#define NECP_POLICY_RESULT_NO_TRIGGER_SCOPED    11      // Deprecated
#define NECP_POLICY_RESULT_SOCKET_SCOPED                12      // String, interface name
#define NECP_POLICY_RESULT_ROUTE_RULES                  13      // N/A, must have route rules defined
#define NECP_POLICY_RESULT_USE_NETAGENT                 14      // netagent uuid_t
#define NECP_POLICY_RESULT_NETAGENT_SCOPED              15      // netagent uuid_t
#define NECP_POLICY_RESULT_SCOPED_DIRECT                16      // N/A, scopes to primary physical interface
#define NECP_POLICY_RESULT_ALLOW_UNENTITLED             17      // N/A
#define NECP_POLICY_RESULT_REMOVE_NETAGENT              18      // netagent uuid_t

#define NECP_POLICY_RESULT_MAX                          NECP_POLICY_RESULT_REMOVE_NETAGENT

/*
 * PASS Result Flags
 */
#define NECP_POLICY_PASS_NO_SKIP_IPSEC                  0x01
#define NECP_POLICY_PASS_PF_TAG                         0x02

/*
 * DROP Result Flags
 */
#define NECP_POLICY_DROP_FLAG_LOCAL_NETWORK             0x01
#define NECP_POLICY_DROP_FLAG_SUPPRESS_ALERTS           0x02

/*
 * Local-Networks Condition Flags
 */
#define NECP_POLICY_LOCAL_NETWORKS_FLAG_INCLUDE_LOCAL_ADDRESSES 0x01     // Include addresses on local interfaces for Local-Networks condition

/*
 * Route Rules
 * Detailed parameters for NECP_POLICY_RESULT_ROUTE_RULES.
 */
#define NECP_ROUTE_RULE_NONE                                    0       // N/A
#define NECP_ROUTE_RULE_DENY_INTERFACE                  1       // String, or empty to match all
#define NECP_ROUTE_RULE_ALLOW_INTERFACE                 2       // String, or empty to match all
#define NECP_ROUTE_RULE_QOS_MARKING                             3       // String, or empty to match all
#define NECP_ROUTE_RULE_DENY_LQM_ABORT                  4       // String, or empty to match all
#define NECP_ROUTE_RULE_USE_NETAGENT                    5       // UUID, followed by string or empty
#define NECP_ROUTE_RULE_REMOVE_NETAGENT                 6       // UUID, followed by string or empty
#define NECP_ROUTE_RULE_DIVERT_SOCKET                   7       // u_int32_t control unit, followed by string or empty
#define NECP_ROUTE_RULE_DENY_INTERFACE_WITH_TYPE        8       // u_int32_t effective type, followed by string or empty

#define NECP_ROUTE_RULE_FLAG_CELLULAR                   0x01
#define NECP_ROUTE_RULE_FLAG_WIFI                       0x02
#define NECP_ROUTE_RULE_FLAG_WIRED                      0x04
#define NECP_ROUTE_RULE_FLAG_EXPENSIVE                  0x08
#define NECP_ROUTE_RULE_FLAG_CONSTRAINED                0x10
#define NECP_ROUTE_RULE_FLAG_COMPANION                  0x20
#define NECP_ROUTE_RULE_FLAG_VPN                        0x40

#define NECP_ROUTE_RULE_FLAG_NETAGENT                   0x80 // Last bit, reserved to mark that this applies only when an agent UUID is present

#define NECP_ROUTE_RULES_SUPPORT_NETAGENT_EXCEPTIONS    1

/*
 * Error types
 */
#define NECP_ERROR_INTERNAL                                             0
#define NECP_ERROR_UNKNOWN_PACKET_TYPE                  1
#define NECP_ERROR_INVALID_TLV                                  2
#define NECP_ERROR_POLICY_RESULT_INVALID                3
#define NECP_ERROR_POLICY_CONDITIONS_INVALID    4
#define NECP_ERROR_POLICY_ID_NOT_FOUND                  5
#define NECP_ERROR_INVALID_PROCESS                              6
#define NECP_ERROR_ROUTE_RULES_INVALID                  7

// Modifiers
#define NECP_MASK_USERSPACE_ONLY        0x80000000      // on filter_control_unit value
#define NECP_MASK_PRESERVE_CONNECTIONS  0x20000000      // on filter_control_unit value

struct necp_policy_condition_tc_range {
	u_int32_t start_tc;
	u_int32_t end_tc;
} __attribute__((__packed__));

struct necp_policy_condition_addr {
	u_int8_t                prefix;
	union {
		struct sockaddr                 sa;
		struct sockaddr_in              sin;
		struct sockaddr_in6             sin6;
	} address __attribute__((__packed__));
} __attribute__((__packed__));

struct necp_policy_condition_addr_range {
	union {
		struct sockaddr                 sa;
		struct sockaddr_in              sin;
		struct sockaddr_in6             sin6;
	} start_address __attribute__((__packed__));
	union {
		struct sockaddr                 sa;
		struct sockaddr_in              sin;
		struct sockaddr_in6             sin6;
	} end_address __attribute__((__packed__));
} __attribute__((__packed__));

struct necp_policy_condition_agent_type {
	char agent_domain[32];
	char agent_type[32];
} __attribute__((__packed__));

struct necp_policy_condition_sdk_version {
	uint32_t platform; // e.g., PLATFORM_IOS
	uint32_t min_version; // Encoded as XXXX.YY.ZZ
	uint32_t version; // Encoded as XXXX.YY.ZZ
} __attribute__((__packed__));

#define NECP_SESSION_PRIORITY_UNKNOWN                   0
#define NECP_SESSION_PRIORITY_CONTROL                   1
#define NECP_SESSION_PRIORITY_CONTROL_1                 2
#define NECP_SESSION_PRIORITY_PRIVILEGED_TUNNEL         3
#define NECP_SESSION_PRIORITY_HIGH                      4
#define NECP_SESSION_PRIORITY_HIGH_1                    5
#define NECP_SESSION_PRIORITY_HIGH_2                    6
#define NECP_SESSION_PRIORITY_HIGH_3                    7
#define NECP_SESSION_PRIORITY_HIGH_4                    8
#define NECP_SESSION_PRIORITY_HIGH_RESTRICTED           9
#define NECP_SESSION_PRIORITY_DEFAULT                   10
#define NECP_SESSION_PRIORITY_LOW                       11
#define NECP_SESSION_NUM_PRIORITIES                             NECP_SESSION_PRIORITY_LOW

typedef u_int32_t necp_policy_id;
typedef u_int32_t necp_policy_order;
typedef u_int32_t necp_session_priority;

typedef u_int32_t necp_kernel_policy_result;
typedef u_int32_t necp_kernel_policy_filter;

typedef union {
	u_int                                           tunnel_interface_index;
	u_int                                           scoped_interface_index;
	u_int32_t                                       flow_divert_control_unit;
	u_int32_t                                       filter_control_unit;
	u_int32_t                                       pass_flags;
	u_int32_t                                       drop_flags;
} necp_kernel_policy_routing_result_parameter;

#define NECP_SERVICE_FLAGS_REGISTERED                   0x01
#define NECP_MAX_NETAGENTS                                              16

#define NECP_AGENT_USE_FLAG_SCOPE                               0x01
#define NECP_AGENT_USE_FLAG_REMOVE                              0x02

#define NECP_TFO_COOKIE_LEN_MAX      16
struct necp_aggregate_result {
	necp_kernel_policy_result                       routing_result;
	necp_kernel_policy_routing_result_parameter     routing_result_parameter;
	necp_kernel_policy_filter                       filter_control_unit;
	u_int32_t                                       flow_divert_aggregate_unit;
	necp_kernel_policy_result                       service_action;
	uuid_t                                                          service_uuid;
	u_int32_t                                                       service_flags;
	u_int32_t                                                       service_data;
	u_int                                                           routed_interface_index;
	u_int32_t                                                       policy_id;
	u_int32_t                                                       skip_policy_id;
	uuid_t                                                          netagents[NECP_MAX_NETAGENTS];
	u_int32_t                                                       netagent_use_flags[NECP_MAX_NETAGENTS];
	struct ipv6_prefix                                              nat64_prefixes[NAT64_MAX_NUM_PREFIXES];
	u_int8_t                                                        mss_recommended;
};

/*
 * Statistics.  It would be nice if the definitions in ntstat.h could be used,
 * but they get entangled with #defines for v4 etc in pfvar.h and it may be better practice
 * to have separate definitions here.
 */
struct necp_stat_counts {
	/*	Counters	*/
	u_int64_t       necp_stat_rxpackets             __attribute__((aligned(8)));
	u_int64_t       necp_stat_rxbytes               __attribute__((aligned(8)));
	u_int64_t       necp_stat_txpackets             __attribute__((aligned(8)));
	u_int64_t       necp_stat_txbytes               __attribute__((aligned(8)));

	u_int32_t       necp_stat_rxduplicatebytes;
	u_int32_t       necp_stat_rxoutoforderbytes;
	u_int32_t       necp_stat_txretransmit;

	u_int32_t       necp_stat_connectattempts;
	u_int32_t       necp_stat_connectsuccesses;

	u_int32_t       necp_stat_min_rtt;
	u_int32_t       necp_stat_avg_rtt;
	u_int32_t       necp_stat_var_rtt;

#define NECP_STAT_ROUTE_FLAGS   1
	u_int32_t       necp_stat_route_flags;
};

// Note, some metadata is implicit in the necp client itself:
// From the process itself : pid, upid, uuid, proc name.
// From the necp client parameters: local and remote addresses, euuid, traffic class, ifindex
//
// The following may well be supplied via future necp client parameters,
// but they are here so they don't get forgotten.
struct necp_basic_metadata {
	u_int32_t       rcvbufsize;
	u_int32_t       rcvbufused;
};

struct necp_connection_probe_status {
	unsigned int    probe_activated : 1;
	unsigned int    write_probe_failed : 1;
	unsigned int    read_probe_failed : 1;
	unsigned int    conn_probe_failed : 1;
};

struct necp_extra_tcp_metadata {
	struct necp_connection_probe_status probestatus;

	u_int32_t       sndbufsize;
	u_int32_t       sndbufused;
	u_int32_t       txunacked;
	u_int32_t       txwindow;
	u_int32_t       txcwindow;
	u_int32_t       flags;                  // use SOF_*
	u_int32_t       flags1;                 // use SOF1_*
	u_int32_t       traffic_mgt_flags;
	u_int32_t       cc_alg_index;
	u_int32_t       state;
};

struct necp_stats_hdr {
	u_int32_t                                       necp_stats_type __attribute__((aligned(8)));
	u_int32_t                                       necp_stats_ver;
	u_int64_t                                       __necp_stats_reserved; // Pad the field for future use
};

#define NECP_CLIENT_STATISTICS_TYPE_TCP                         1       // Identifies use of necp_tcp_stats
#define NECP_CLIENT_STATISTICS_TYPE_UDP                         2       // Identifies use of necp_udp_stats
#define NECP_CLIENT_STATISTICS_TYPE_QUIC                        3       // Identifies use of necp_quic_stats

#define NECP_CLIENT_STATISTICS_TYPE_TCP_VER_1           1       // Currently supported version for TCP
#define NECP_CLIENT_STATISTICS_TYPE_UDP_VER_1           1       // Currently supported version for UDP
#define NECP_CLIENT_STATISTICS_TYPE_QUIC_VER_1          1       // Currently supported version for QUIC

#define NECP_CLIENT_STATISTICS_TYPE_TCP_CURRENT_VER             NECP_CLIENT_STATISTICS_TYPE_TCP_VER_1
#define NECP_CLIENT_STATISTICS_TYPE_UDP_CURRENT_VER             NECP_CLIENT_STATISTICS_TYPE_UDP_VER_1
#define NECP_CLIENT_STATISTICS_TYPE_QUIC_CURRENT_VER            NECP_CLIENT_STATISTICS_TYPE_QUIC_VER_1

#define NECP_CLIENT_STATISTICS_EVENT_INIT                       0x00000000              // Register the flow
#define NECP_CLIENT_STATISTICS_EVENT_TIME_WAIT          0x00000001              // The flow is effectively finished but waiting on timer

struct necp_tcp_stats {
	struct necp_stats_hdr                   necp_tcp_hdr;
	struct necp_stat_counts                 necp_tcp_counts;
	struct necp_basic_metadata              necp_tcp_basic;
	struct necp_extra_tcp_metadata  necp_tcp_extra;
};

struct necp_udp_stats {
	struct necp_stats_hdr           necp_udp_hdr;
	struct necp_stat_counts         necp_udp_counts;
	struct necp_basic_metadata      necp_udp_basic;
};


/*
 * The following reflects the special case for QUIC.
 * It is a streaming protocol built on top of UDP.
 * Therefore QUIC stats are defined as basic UDP stats
 * with some extra meta data.
 * TODO: For now the extra metadata is an exact replica
 * of the metadata for TCP. However keeping that separate allows
 * the structures to diverge later as new stats are added.
 */
#define QUIC_STATELESS_RESET_TOKEN_SIZE               16
#define NECP_QUIC_HAS_PROBE_STATUS 1
struct necp_extra_quic_metadata {
	u_int32_t       sndbufsize;
	u_int32_t       sndbufused;
	u_int32_t       txunacked;
	u_int32_t       txwindow;
	u_int32_t       txcwindow;
	u_int32_t       traffic_mgt_flags;
	u_int32_t       cc_alg_index;
	u_int32_t       state;
	u_int8_t        ssr_token[QUIC_STATELESS_RESET_TOKEN_SIZE];
	struct necp_connection_probe_status probestatus;
};

#define necp_quic_hdr           necp_quic_udp_stats.necp_udp_hdr
#define necp_quic_counts        necp_quic_udp_stats.necp_udp_counts
#define necp_quic_basic         necp_quic_udp_stats.necp_udp_basic
struct necp_quic_stats {
	struct necp_udp_stats           necp_quic_udp_stats;
	struct necp_extra_quic_metadata necp_quic_extra;
};

typedef struct necp_all_stats {
	union {
		struct necp_tcp_stats   tcp_stats;
		struct necp_udp_stats   udp_stats;
		struct necp_quic_stats  quic_stats;
	} all_stats_u;
} necp_all_stats;

// Memory for statistics is requested via a necp_stats_bufreq
//
struct necp_stats_bufreq {
	u_int32_t                                       necp_stats_bufreq_id __attribute__((aligned(8)));
	u_int32_t                                       necp_stats_bufreq_type;         //  NECP_CLIENT_STATISTICS_TYPE_*
	u_int32_t                                       necp_stats_bufreq_ver;          //  NECP_CLIENT_STATISTICS_TYPE_*_VER
	u_int32_t                                       necp_stats_bufreq_size;
	union {
		void                                    *necp_stats_bufreq_addr;
		mach_vm_address_t               necp_stats_bufreq_uaddr;
	};
};

#define NECP_CLIENT_STATISTICS_BUFREQ_ID                                0xbf    // Distinguishes from statistics actions taking a necp_all_stats struct

// There is a limit to the number of statistics structures that may be allocated per process, subject to change
//
#define NECP_MAX_PER_PROCESS_CLIENT_STATISTICS_STRUCTS  512

#define NECP_TCP_ECN_HEURISTICS_SYN_RST 1
typedef struct necp_tcp_ecn_cache {
	u_int8_t                necp_tcp_ecn_heuristics_success:1;
	u_int8_t                necp_tcp_ecn_heuristics_loss:1;
	u_int8_t                necp_tcp_ecn_heuristics_drop_rst:1;
	u_int8_t                necp_tcp_ecn_heuristics_drop_rxmt:1;
	u_int8_t                necp_tcp_ecn_heuristics_aggressive:1;
	u_int8_t                necp_tcp_ecn_heuristics_syn_rst:1;
} necp_tcp_ecn_cache;

#define NECP_TCP_TFO_HEURISTICS_RST 1
typedef struct necp_tcp_tfo_cache {
	u_int8_t                necp_tcp_tfo_cookie[NECP_TFO_COOKIE_LEN_MAX];
	u_int8_t                necp_tcp_tfo_cookie_len;
	u_int8_t                necp_tcp_tfo_heuristics_success:1; // TFO succeeded with data in the SYN
	u_int8_t                necp_tcp_tfo_heuristics_loss:1; // TFO SYN-loss with data
	u_int8_t                necp_tcp_tfo_heuristics_middlebox:1; // TFO middlebox detected
	u_int8_t                necp_tcp_tfo_heuristics_success_req:1; // TFO succeeded with the TFO-option in the SYN
	u_int8_t                necp_tcp_tfo_heuristics_loss_req:1; // TFO SYN-loss with the TFO-option
	u_int8_t                necp_tcp_tfo_heuristics_rst_data:1; // Recevied RST upon SYN with data in the SYN
	u_int8_t                necp_tcp_tfo_heuristics_rst_req:1; // Received RST upon SYN with the TFO-option
} necp_tcp_tfo_cache;

#define NECP_CLIENT_CACHE_TYPE_ECN                 1       // Identifies use of necp_tcp_ecn_cache
#define NECP_CLIENT_CACHE_TYPE_TFO                 2       // Identifies use of necp_tcp_tfo_cache

#define NECP_CLIENT_CACHE_TYPE_ECN_VER_1           1       // Currently supported version for ECN
#define NECP_CLIENT_CACHE_TYPE_TFO_VER_1           1       // Currently supported version for TFO

typedef struct necp_cache_buffer {
	u_int8_t                necp_cache_buf_type;    //  NECP_CLIENT_CACHE_TYPE_*
	u_int8_t                necp_cache_buf_ver;     //  NECP_CLIENT_CACHE_TYPE_*_VER
	u_int32_t               necp_cache_buf_size;
	mach_vm_address_t       necp_cache_buf_addr;
} necp_cache_buffer;

/*
 * NECP Client definitions
 */
#define NECP_MAX_CLIENT_PARAMETERS_SIZE                                 1024
#define NECP_MAX_CLIENT_RESULT_SIZE                                     512 // Legacy
#define NECP_BASE_CLIENT_RESULT_SIZE                                    1024
#define NECP_CLIENT_FLOW_RESULT_SIZE                                    512

#define NECP_OPEN_FLAG_OBSERVER                                                 0x01 // Observers can query clients they don't own
#define NECP_OPEN_FLAG_BACKGROUND                                               0x02 // Mark this fd as backgrounded
#define NECP_OPEN_FLAG_PUSH_OBSERVER                                    0x04 // When used with the OBSERVER flag, allows updates to be pushed. Adding clients is not allowed in this mode.

#define NECP_FD_SUPPORTS_GUARD                                                  1

#define NECP_CLIENT_ACTION_ADD                                                  1 // Register a new client. Input: parameters in buffer; Output: client_id
#define NECP_CLIENT_ACTION_REMOVE                                               2 // Unregister a client. Input: client_id, optional struct ifnet_stats_per_flow
#define NECP_CLIENT_ACTION_COPY_PARAMETERS                              3 // Copy client parameters. Input: client_id; Output: parameters in buffer
#define NECP_CLIENT_ACTION_COPY_RESULT                                  4 // Copy client result. Input: client_id; Output: result in buffer
#define NECP_CLIENT_ACTION_COPY_LIST                                    5 // Copy all client IDs. Output: struct necp_client_list in buffer
#define NECP_CLIENT_ACTION_REQUEST_NEXUS_INSTANCE               6 // Request a nexus instance from a nexus provider, optional struct necp_stats_bufreq
#define NECP_CLIENT_ACTION_AGENT                                                7 // Interact with agent. Input: client_id, agent parameters
#define NECP_CLIENT_ACTION_COPY_AGENT                                   8 // Copy agent content. Input: agent UUID; Output: struct netagent
#define NECP_CLIENT_ACTION_COPY_INTERFACE                               9 // Copy interface details. Input: ifindex cast to UUID; Output: struct necp_interface_details
#define NECP_CLIENT_ACTION_SET_STATISTICS                               10 // Deprecated
#define NECP_CLIENT_ACTION_COPY_ROUTE_STATISTICS                11 // Get route statistics. Input: client_id; Output: struct necp_stat_counts
#define NECP_CLIENT_ACTION_AGENT_USE                                    12 // Return the use count and increment the use count. Input/Output: struct necp_agent_use_parameters
#define NECP_CLIENT_ACTION_MAP_SYSCTLS                                  13 // Get the read-only sysctls memory location. Output: mach_vm_address_t
#define NECP_CLIENT_ACTION_UPDATE_CACHE                                 14 // Update heuristics and cache
#define NECP_CLIENT_ACTION_COPY_CLIENT_UPDATE                   15 // Fetch an updated client for push-mode observer. Output: Client id, struct necp_client_observer_update in buffer
#define NECP_CLIENT_ACTION_COPY_UPDATED_RESULT                  16 // Copy client result only if changed. Input: client_id; Output: result in buffer
#define NECP_CLIENT_ACTION_ADD_FLOW                                             17 // Add a flow. Input: client_id; Output: struct necp_client_add_flow
#define NECP_CLIENT_ACTION_REMOVE_FLOW                                  18 // Remove a flow. Input: flow_id, optional struct ifnet_stats_per_flow
#define NECP_CLIENT_ACTION_CLAIM                                       19 // Claim a client that has been added for this unique PID. Input: client_id
#define NECP_CLIENT_ACTION_SIGN                                       20 // Sign a query answer. Input: struct necp_client_signable; Output: struct necp_client_signature
#define NECP_CLIENT_ACTION_GET_INTERFACE_ADDRESS                       21 // Get the best interface local address for given remote address. Input: ifindex, remote sockaddr; Output: matching local sockaddr
#define NECP_CLIENT_ACTION_ACQUIRE_AGENT_TOKEN                         22 // Get a one-time use token from an agent. Input: agent UUID; Output: token buffer
#define NECP_CLIENT_ACTION_VALIDATE                                    23 // Validate a query answer. Input: struct necp_client_validatable; Output: None
#define NECP_CLIENT_ACTION_GET_SIGNED_CLIENT_ID                        24 // Get a client ID for the appliction along with a signature.
#define NECP_CLIENT_ACTION_SET_SIGNED_CLIENT_ID                        25 // Set a client ID for the appliction along with a signature.
#define NECP_CLIENT_ACTION_COPY_UPDATED_RESULT_FINAL                   26 // Copy client result only if changed, discard data if buffer is too small. Input: client_id; Output: result in buffer

#define NECP_CLIENT_PARAMETER_APPLICATION                               NECP_POLICY_CONDITION_APPLICATION               // Requires entitlement
#define NECP_CLIENT_PARAMETER_REAL_APPLICATION                  NECP_POLICY_CONDITION_REAL_APPLICATION  // Requires entitlement
#define NECP_CLIENT_PARAMETER_DOMAIN                                    NECP_POLICY_CONDITION_DOMAIN
#define NECP_CLIENT_PARAMETER_DOMAIN_OWNER                              NECP_POLICY_CONDITION_DOMAIN_OWNER
#define NECP_CLIENT_PARAMETER_DOMAIN_CONTEXT                            NECP_POLICY_CONDITION_DOMAIN_CONTEXT
#define NECP_CLIENT_PARAMETER_TRACKER_DOMAIN                            NECP_POLICY_CONDITION_TRACKER_DOMAIN
#define NECP_CLIENT_PARAMETER_URL                                       NECP_POLICY_CONDITION_URL
#define NECP_CLIENT_PARAMETER_ATTRIBUTED_BUNDLE_IDENTIFIER              NECP_POLICY_CONDITION_ATTRIBUTED_BUNDLE_IDENTIFIER
#define NECP_CLIENT_PARAMETER_ACCOUNT                                   NECP_POLICY_CONDITION_ACCOUNT
#define NECP_CLIENT_PARAMETER_PID                                               NECP_POLICY_CONDITION_PID                               // Requires entitlement
#define NECP_CLIENT_PARAMETER_UID                                               NECP_POLICY_CONDITION_UID                               // Requires entitlement
#define NECP_CLIENT_PARAMETER_BOUND_INTERFACE                   NECP_POLICY_CONDITION_BOUND_INTERFACE
#define NECP_CLIENT_PARAMETER_TRAFFIC_CLASS                             NECP_POLICY_CONDITION_TRAFFIC_CLASS
#define NECP_CLIENT_PARAMETER_IP_PROTOCOL                               NECP_POLICY_CONDITION_IP_PROTOCOL
#define NECP_CLIENT_PARAMETER_LOCAL_ADDRESS                             NECP_POLICY_CONDITION_LOCAL_ADDR
#define NECP_CLIENT_PARAMETER_REMOTE_ADDRESS                    NECP_POLICY_CONDITION_REMOTE_ADDR
#define NECP_CLIENT_PARAMETER_SCHEME_PORT                                               NECP_POLICY_CONDITION_SCHEME_PORT
#define NECP_CLIENT_PARAMETER_APPLICATION_ID                            NECP_POLICY_CONDITION_APPLICATION_ID
#define NECP_CLIENT_PARAMETER_NEXUS_KEY                                 102

// "Prohibit" will never choose an interface with that property
#define NECP_CLIENT_PARAMETER_PROHIBIT_INTERFACE                100             // String, interface name
#define NECP_CLIENT_PARAMETER_PROHIBIT_IF_TYPE                  101             // u_int8_t, see ifru_functional_type in <net/if.h>
#define NECP_CLIENT_PARAMETER_PROHIBIT_AGENT                    102             // uuid_t, network agent UUID
#define NECP_CLIENT_PARAMETER_PROHIBIT_AGENT_TYPE               103             // struct necp_client_parameter_netagent_type

// "Require" will choose an interface with that property, or none if not found
#define NECP_CLIENT_PARAMETER_REQUIRE_IF_TYPE                   111             // u_int8_t, see ifru_functional_type in <net/if.h>
#define NECP_CLIENT_PARAMETER_REQUIRE_AGENT                             112             // uuid_t, network agent UUID
#define NECP_CLIENT_PARAMETER_REQUIRE_AGENT_TYPE                113             // struct necp_client_parameter_netagent_type

// "Prefer" will choose an interface with an agent, or best otherwise if not found
#define NECP_CLIENT_PARAMETER_PREFER_AGENT                              122             // uuid_t, network agent UUID
#define NECP_CLIENT_PARAMETER_PREFER_AGENT_TYPE                 123             // struct necp_client_parameter_netagent_type

// "Avoid" will choose an interface without an agent, or best otherwise if unavoidable
#define NECP_CLIENT_PARAMETER_AVOID_AGENT                               124             // uuid_t, network agent UUID
#define NECP_CLIENT_PARAMETER_AVOID_AGENT_TYPE                  125             // struct necp_client_parameter_netagent_type

// Use actions with NECP_CLIENT_ACTION_AGENT
#define NECP_CLIENT_PARAMETER_TRIGGER_AGENT                             130             // uuid_t, network agent UUID
#define NECP_CLIENT_PARAMETER_ASSERT_AGENT                              131             // uuid_t, network agent UUID
#define NECP_CLIENT_PARAMETER_UNASSERT_AGENT                    132             // uuid_t, network agent UUID
#define NECP_CLIENT_PARAMETER_AGENT_ADD_GROUP_MEMBERS                    133             // struct necp_client_group_action
#define NECP_CLIENT_PARAMETER_AGENT_REMOVE_GROUP_MEMBERS                    134             // struct necp_client_group_action
#define NECP_CLIENT_PARAMETER_REPORT_AGENT_ERROR                    135             // int32_t

#define NECP_CLIENT_PARAMETER_FALLBACK_MODE                     140             // u_int8_t, see SO_FALLBACK_MODE_* values

#define NECP_CLIENT_PARAMETER_PARENT_ID                                                 150 // uuid_t, client UUID

#define NECP_CLIENT_PARAMETER_LOCAL_ENDPOINT                    200             // struct necp_client_endpoint
#define NECP_CLIENT_PARAMETER_REMOTE_ENDPOINT                   201             // struct necp_client_endpoint
#define NECP_CLIENT_PARAMETER_BROWSE_DESCRIPTOR                  202             // struct necp_client_endpoint
#define NECP_CLIENT_PARAMETER_RESOLVER_TAG                      203                             // struct necp_client_validatable
#define NECP_CLIENT_PARAMETER_ADVERTISE_DESCRIPTOR                  204             // struct necp_client_endpoint
#define NECP_CLIENT_PARAMETER_GROUP_DESCRIPTOR                  205             // struct necp_client_group

#define NECP_CLIENT_PARAMETER_DELEGATED_UPID                              210 // u_int64_t, requires entitlement

#define NECP_CLIENT_PARAMETER_ETHERTYPE                              220 // u_int16_t, ethertype
#define NECP_CLIENT_PARAMETER_TRANSPORT_PROTOCOL                        221 // u_int8_t, IPPROTO_

#define NECP_CLIENT_PARAMETER_LOCAL_ADDRESS_PREFERENCE                        230 // u_int8_t, NECP_CLIENT_PARAMETER_LOCAL_ADDRESS_PREFERENCE_

#define NECP_CLIENT_PARAMETER_LOCAL_ADDRESS_PREFERENCE_DEFAULT                    0
#define NECP_CLIENT_PARAMETER_LOCAL_ADDRESS_PREFERENCE_TEMPORARY                    1
#define NECP_CLIENT_PARAMETER_LOCAL_ADDRESS_PREFERENCE_STABLE                    2

#define NECP_CLIENT_PARAMETER_PERSONA_ID                                        231 // Used to send persona to agents

#define NECP_CLIENT_PARAMETER_FLAGS                                             250             // u_int32_t, see NECP_CLIENT_PAREMETER_FLAG_* values
#define NECP_CLIENT_PARAMETER_FLOW_DEMUX_PATTERN                                251 // struct necp_demux_pattern

#define NECP_CLIENT_PARAMETER_FLAG_MULTIPATH                    0x0001  // Get multipath interface results
#define NECP_CLIENT_PARAMETER_FLAG_BROWSE                               0x0002  // Agent assertions on nexuses are requests to browse
#define NECP_CLIENT_PARAMETER_FLAG_PROHIBIT_EXPENSIVE   0x0004  // Prohibit expensive interfaces
#define NECP_CLIENT_PARAMETER_FLAG_LISTENER                             0x0008  // Client is interested in listening for inbound connections
#define NECP_CLIENT_PARAMETER_FLAG_DISCRETIONARY                0x0010  // Client's traffic is discretionary, and eligible for early defuncting
#define NECP_CLIENT_PARAMETER_FLAG_ECN_ENABLE                   0x0020  // Client is requesting to enable ECN
#define NECP_CLIENT_PARAMETER_FLAG_ECN_DISABLE                  0x0040  // Client is requesting to disable ECN
#define NECP_CLIENT_PARAMETER_FLAG_TFO_ENABLE                   0x0080  // Client is requesting to enable TFO
#define NECP_CLIENT_PARAMETER_FLAG_ONLY_PRIMARY_REQUIRES_TYPE   0x0100    // Interpret NECP_CLIENT_PARAMETER_REQUIRE_IF_TYPE only for primary interface, and allow exceptions for multipath or listeners
#define NECP_CLIENT_PARAMETER_FLAG_CUSTOM_ETHER                 0x0200  // Client expects to open a custom ethernet channel
#define NECP_CLIENT_PARAMETER_FLAG_CUSTOM_IP                    0x0400  // Client expects to open a custom IP protocol channel
#define NECP_CLIENT_PARAMETER_FLAG_INTERPOSE                    0x0800  // Client expects to open an interpose filter channel
#define NECP_CLIENT_PARAMETER_FLAG_PROHIBIT_CONSTRAINED       0x1000  // Prohibit constrained interfaces
#define NECP_CLIENT_PARAMETER_FLAG_FALLBACK_TRAFFIC           0x2000  // Fallback traffic
#define NECP_CLIENT_PARAMETER_FLAG_INBOUND                    0x4000  // Flow is inbound (passive)
#define NECP_CLIENT_PARAMETER_FLAG_SYSTEM_PROXY               0x8000  // Flow is a system proxy
#define NECP_CLIENT_PARAMETER_FLAG_KNOWN_TRACKER              0x10000  // Flow is contacting a known tracker
#define NECP_CLIENT_PARAMETER_FLAG_UNSAFE_SOCKET_ACCESS       0x20000  // Client allows direct access to sockets
#define NECP_CLIENT_PARAMETER_FLAG_NON_APP_INITIATED          0x40000  // Networking activities not initiated by application
#define NECP_CLIENT_PARAMETER_FLAG_THIRD_PARTY_WEB_CONTENT    0x80000  // Third-party web content, not main load
#define NECP_CLIENT_PARAMETER_FLAG_SILENT                    0x100000  // Private browsing - do not log/track
#define NECP_CLIENT_PARAMETER_FLAG_APPROVED_APP_DOMAIN       0x200000  // Approved associated app domain; domain is "owned" by app
#define NECP_CLIENT_PARAMETER_FLAG_NO_WAKE_FROM_SLEEP        0x400000  // Don't wake from sleep on traffic for this client
#define NECP_CLIENT_PARAMETER_FLAG_REUSE_LOCAL               0x800000  // Request support for local address/port reuse
#define NECP_CLIENT_PARAMETER_FLAG_ENHANCED_PRIVACY         0x1000000  // Attempt protocol upgrade or proxy usage for more privacy
#define NECP_CLIENT_PARAMETER_FLAG_WEB_SEARCH_CONTENT       0x2000000  // Web search traffic
#define NECP_CLIENT_PARAMETER_FLAG_ALLOW_ULTRA_CONSTRAINED  0x4000000  // Allow ultra-constrained interfaces
#define NECP_CLIENT_PARAMETER_FLAG_HAS_ACCOUNT_ID           0x8000000  // Client has provided an account identifier

#define NECP_CLIENT_RESULT_CLIENT_ID                                    1               // uuid_t
#define NECP_CLIENT_RESULT_POLICY_RESULT                                2               // u_int32_t
#define NECP_CLIENT_RESULT_POLICY_RESULT_PARAMETER              3               // u_int32_t
#define NECP_CLIENT_RESULT_FILTER_CONTROL_UNIT                  4               // u_int32_t
#define NECP_CLIENT_RESULT_INTERFACE_INDEX                              5               // u_int32_t
#define NECP_CLIENT_RESULT_NETAGENT                                             6               // struct necp_client_result_netagent
#define NECP_CLIENT_RESULT_FLAGS                                                7               // u_int32_t, see NECP_CLIENT_RESULT_FLAG_* values
#define NECP_CLIENT_RESULT_INTERFACE                                    8               // struct necp_client_result_interface
#define NECP_CLIENT_RESULT_INTERFACE_OPTION                             9               // struct necp_client_interface_option
#define NECP_CLIENT_RESULT_EFFECTIVE_MTU                                10              // u_int32_t
#define NECP_CLIENT_RESULT_FLOW                                                 11              // TLV array of a single flow's state
#define NECP_CLIENT_RESULT_PROTO_CTL_EVENT                              12
#define NECP_CLIENT_RESULT_TFO_COOKIE                                   13              // NECP_TFO_COOKIE_LEN_MAX
#define NECP_CLIENT_RESULT_TFO_FLAGS                                    14              // u_int8_t
#define NECP_CLIENT_RESULT_RECOMMENDED_MSS                              15              // u_int8_t
#define NECP_CLIENT_RESULT_FLOW_ID                                              16              // uuid_t
#define NECP_CLIENT_RESULT_INTERFACE_TIME_DELTA                 17              // u_int32_t, seconds since interface up/down
#define NECP_CLIENT_RESULT_REASON                                               18              // u_int32_t, see NECP_CLIENT_RESULT_REASON_* values
#define NECP_CLIENT_RESULT_FLOW_DIVERT_AGGREGATE_UNIT                   19              // u_int32_t
#define NECP_CLIENT_RESULT_REQUEST_IN_PROCESS_FLOW_DIVERT               20              // Empty

#define NECP_CLIENT_RESULT_NEXUS_INSTANCE                               100             // uuid_t
#define NECP_CLIENT_RESULT_NEXUS_PORT                                   101             // nexus_port_t
#define NECP_CLIENT_RESULT_NEXUS_KEY                                    102             // uuid_t
#define NECP_CLIENT_RESULT_NEXUS_PORT_FLOW_INDEX                103             // u_int32_t
#define NECP_CLIENT_RESULT_NEXUS_FLOW_STATS                             104             // struct sk_stats_flow *

#define NECP_CLIENT_RESULT_LOCAL_ENDPOINT                               200             // struct necp_client_endpoint
#define NECP_CLIENT_RESULT_REMOTE_ENDPOINT                              201             // struct necp_client_endpoint
#define NECP_CLIENT_RESULT_DISCOVERED_ENDPOINT                  202             // struct necp_client_endpoint, result of browse
#define NECP_CLIENT_RESULT_RESOLVED_ENDPOINT                  203             // struct necp_client_endpoint, result of resolve
#define NECP_CLIENT_RESULT_LOCAL_ETHER_ADDR                                     204                     // struct ether_addr
#define NECP_CLIENT_RESULT_REMOTE_ETHER_ADDR                                    205                     // struct ether_addr
#define NECP_CLIENT_RESULT_EFFECTIVE_TRAFFIC_CLASS              210             // u_int32_t
#define NECP_CLIENT_RESULT_TRAFFIC_MGMT_BG                              211             // u_int32_t, 1: background, 0: not background
#define NECP_CLIENT_RESULT_GATEWAY                                      212             // struct necp_client_endpoint
#define NECP_CLIENT_RESULT_GROUP_MEMBER                                      213             // struct necp_client_endpoint
#define NECP_CLIENT_RESULT_NAT64                                        214             // struct ipv6_prefix[NAT64_MAX_NUM_PREFIXES]
#define NECP_CLIENT_RESULT_ESTIMATED_THROUGHPUT                         215             // struct necp_client_result_estimated_throughput
#define NECP_CLIENT_RESULT_AGENT_ERROR                                  216             // struct necp_client_result_agent_error

#define NECP_CLIENT_RESULT_FLAG_IS_LOCAL                                0x0001  // Routes to this device
#define NECP_CLIENT_RESULT_FLAG_IS_DIRECT                               0x0002  // Routes to directly accessible peer
#define NECP_CLIENT_RESULT_FLAG_HAS_IPV4                                0x0004  // Supports IPv4
#define NECP_CLIENT_RESULT_FLAG_HAS_IPV6                                0x0008  // Supports IPv6
#define NECP_CLIENT_RESULT_FLAG_DEFUNCT                                 0x0010  // Defunct
#define NECP_CLIENT_RESULT_FLAG_SATISFIED                               0x0020  // Satisfied path
#define NECP_CLIENT_RESULT_FLAG_FLOW_ASSIGNED                   0x0040  // Assigned, the flow is active
#define NECP_CLIENT_RESULT_FLAG_FLOW_VIABLE                             0x0080  // Viable, the flow has a valid route
#define NECP_CLIENT_RESULT_FLAG_PROBE_CONNECTIVITY              0x0100  // Flow should probe connectivity
#define NECP_CLIENT_RESULT_FLAG_ECN_ENABLED                             0x0200  // ECN should be used
#define NECP_CLIENT_RESULT_FLAG_FAST_OPEN_BLOCKED               0x0400  // Fast open should not be used
#define NECP_CLIENT_RESULT_FLAG_LINK_QUALITY_ABORT              0x0800  // Link quality is very bad, recommend close connections
#define NECP_CLIENT_RESULT_FLAG_ALLOW_QOS_MARKING               0x1000  // QoS marking is allowed
#define NECP_CLIENT_RESULT_FLAG_HAS_NAT64                       0x2000  // Has NAT64 prefix
#define NECP_CLIENT_RESULT_FLAG_INTERFACE_LOW_POWER             0x4000  // Interface is in low-power mode
#define NECP_CLIENT_RESULT_FLAG_SPECIFIC_LISTENER               0x8000  // Listener should not listen on all interfaces
#define NECP_CLIENT_RESULT_FLAG_KEXT_FILTER_PRESENT             0x10000 // Kernel extension filter present
#define NECP_CLIENT_RESULT_FLAG_PF_RULES_PRESENT                0x20000 // Firewall rules present
#define NECP_CLIENT_RESULT_FLAG_ALF_PRESENT                     0x40000 // Application Firewall enabled
#define NECP_CLIENT_RESULT_FLAG_PARENTAL_CONTROLS_PRESENT       0x80000 // Parental Controls present
#define NECP_CLIENT_RESULT_FLAG_IS_GLOBAL_INTERNET              0x100000 // Routes to global Internet


#define NECP_CLIENT_RESULT_FLAG_FORCE_UPDATE (NECP_CLIENT_RESULT_FLAG_HAS_IPV4 | NECP_CLIENT_RESULT_FLAG_HAS_IPV6 | NECP_CLIENT_RESULT_FLAG_HAS_NAT64 | NECP_CLIENT_RESULT_FLAG_INTERFACE_LOW_POWER)

#define NECP_CLIENT_RESULT_FAST_OPEN_SND_PROBE                  0x01    // DEPRECATED - Fast open send probe
#define NECP_CLIENT_RESULT_FAST_OPEN_RCV_PROBE                  0x02    // DEPRECATED - Fast open receive probe

#define NECP_CLIENT_RESULT_RECOMMENDED_MSS_NONE                 0x01
#define NECP_CLIENT_RESULT_RECOMMENDED_MSS_LOW                  0x02
#define NECP_CLIENT_RESULT_RECOMMENDED_MSS_MEDIUM               0x04

#define NECP_CLIENT_RESULT_REASON_EXPENSIVE_PROHIBITED          1  // Expensive networks were prohibited
#define NECP_CLIENT_RESULT_REASON_CONSTRAINED_PROHIBITED                2  // Constrained networks were prohibited
#define NECP_CLIENT_RESULT_REASON_CELLULAR_DENIED                3  // Denied by a cellular route rule
#define NECP_CLIENT_RESULT_REASON_WIFI_DENIED                4  // Denied by a wifi route rule
#define NECP_CLIENT_RESULT_REASON_LOCAL_NETWORK_PROHIBITED       5  // Local network access prohibited
#define NECP_CLIENT_RESULT_REASON_ULTRA_CONSTRAINED_NOT_ALLOWED  6  // Ultra constrained networks not allowed

struct necp_interface_signature {
	u_int8_t signature[IFNET_SIGNATURELEN];
	u_int8_t signature_len;
};

struct necp_interface_details {
	char name[IFXNAMSIZ];
	u_int32_t index;
	u_int32_t generation;
	u_int32_t functional_type;
	u_int32_t delegate_index;
	u_int32_t flags; // see NECP_INTERFACE_FLAG_*
	u_int32_t mtu;
	struct necp_interface_signature ipv4_signature;
	struct necp_interface_signature ipv6_signature;
	u_int32_t ipv4_netmask;
	u_int32_t ipv4_broadcast;
	/*
	 * This serves as a temporary header guard to protect libnetcore builds
	 * until the xnu changes are available in the build.
	 * XXX: Should be removed in a future build.
	 */
#define NECP_INTERFACE_SUPPORTS_TSO    1
	u_int32_t tso_max_segment_size_v4;
	u_int32_t tso_max_segment_size_v6;
#define NECP_INTERFACE_SUPPORTS_HWCSUM   1
	u_int32_t hwcsum_flags;
	u_int8_t  radio_type;
	u_int8_t  radio_channel;
};

#define NECP_INTERFACE_FLAG_EXPENSIVE                                   0x0001
#define NECP_INTERFACE_FLAG_TXSTART                                     0X0002
#define NECP_INTERFACE_FLAG_NOACKPRI                                    0x0004
#define NECP_INTERFACE_FLAG_3CARRIERAGG                                 0x0008
#define NECP_INTERFACE_FLAG_IS_LOW_POWER                                0x0010
#define NECP_INTERFACE_FLAG_MPK_LOG                                     0x0020 // Multi-layer Packet Logging
#define NECP_INTERFACE_FLAG_CONSTRAINED                                 0x0040
#define NECP_INTERFACE_FLAG_HAS_NETMASK                                 0x0080
#define NECP_INTERFACE_FLAG_HAS_BROADCAST                               0x0100
#define NECP_INTERFACE_FLAG_SUPPORTS_MULTICAST                          0x0200
#define NECP_INTERFACE_FLAG_HAS_DNS                                     0x0400
#define NECP_INTERFACE_FLAG_HAS_NAT64                                   0x0800
#define NECP_INTERFACE_FLAG_IPV4_ROUTABLE                               0x1000
#define NECP_INTERFACE_FLAG_IPV6_ROUTABLE                               0x2000
#define NECP_INTERFACE_FLAG_ULTRA_CONSTRAINED                           0x4000

struct necp_client_parameter_netagent_type {
	char netagent_domain[32];
	char netagent_type[32];
};

struct necp_client_result_netagent {
	u_int32_t generation;
	uuid_t netagent_uuid;
};

struct necp_client_result_interface {
	u_int32_t generation;
	u_int32_t index;
};

struct necp_client_result_estimated_throughput {
	u_int8_t up;
	u_int8_t down;
};

struct necp_client_result_agent_error {
	u_int32_t code;
	u_int8_t domain;
};

#define NECP_USES_INTERFACE_OPTIONS_FOR_BROWSE 1

struct necp_client_interface_option {
	u_int32_t interface_index;
	u_int32_t interface_generation;
	uuid_t nexus_agent;
};

#define NECP_CLIENT_ENDPOINT_TYPE_APPLICATION_SERVICE 6

struct necp_client_endpoint {
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
		struct {
			u_int8_t endpoint_length;
			u_int8_t endpoint_family; // Use AF_UNSPEC to target a name
			u_int16_t endpoint_port;
			u_int32_t endpoint_type; // Client-specific type
			char endpoint_data[0]; // Type-specific endpoint value
		} endpoint;
	} u;
};

struct necp_client_group {
	u_int32_t group_type;
	uuid_t group_id;
};

struct necp_client_list {
	u_int32_t client_count;
	uuid_t clients[0];
};

struct kev_necp_policies_changed_data {
	u_int32_t               changed_count;  // Defaults to 0.
};

#define NECP_CLIENT_FLOW_FLAGS_ALLOW_NEXUS                      0x01    // Request a nexus instance upon adding a flow
#define NECP_CLIENT_FLOW_FLAGS_USE_CLIENT_ID            0x02    // Register the client ID rather than the flow registration ID with network agents
#define NECP_CLIENT_FLOW_FLAGS_BROWSE                      0x04    // Create request with a browse agent
#define NECP_CLIENT_FLOW_FLAGS_RESOLVE                      0x08    // Create request with a resolution agent
#define NECP_CLIENT_FLOW_FLAGS_OVERRIDE_ADDRESS                      0x10    // Flow has a different remote address than the parent flow
#define NECP_CLIENT_FLOW_FLAGS_OVERRIDE_IP_PROTOCOL           0x20    // Flow has a different IP protocol than the parent flow

struct necp_client_flow_stats {
	u_int32_t stats_type; // NECP_CLIENT_STATISTICS_TYPE_*
	u_int32_t stats_version; // NECP_CLIENT_STATISTICS_TYPE_*_VER
	u_int32_t stats_size;
	mach_vm_address_t stats_addr;
};

struct necp_client_add_flow {
	uuid_t agent_uuid;
	uuid_t registration_id;
	u_int16_t flags; // NECP_CLIENT_FLOW_FLAGS_*
	u_int16_t stats_request_count;
	struct necp_client_flow_stats stats_requests[0];
	// sockaddr for override endpoint
	// uint8 for override ip protocol
} __attribute__((__packed__));

struct necp_agent_use_parameters {
	uuid_t agent_uuid;
	uint64_t out_use_count;
};

struct necp_client_group_action {
	uuid_t agent_uuid;
	u_int16_t group_member_count;
	struct necp_client_endpoint group_members[0];
} __attribute__((__packed__));

struct necp_client_flow_protoctl_event {
	uint32_t        protoctl_event_code;
	uint32_t        protoctl_event_val;
	/* TCP seq number is in host byte order */
	uint32_t        protoctl_event_tcp_seq_num;
};

#define NECP_CLIENT_UPDATE_TYPE_PARAMETERS              1       // Parameters, for a new client
#define NECP_CLIENT_UPDATE_TYPE_RESULT                  2       // Result, for a udpated client
#define NECP_CLIENT_UPDATE_TYPE_REMOVE                  3       // Empty, for a removed client

struct necp_client_observer_update {
	u_int32_t update_type;  // NECP_CLIENT_UPDATE_TYPE_*
	u_int8_t tlv_buffer[0]; // Parameters or result as TLVs, based on type
};

// These sign types are tied to specific clients.
// The client_id UUID is then the NECP client UUID that generated the query.
#define NECP_CLIENT_SIGN_TYPE_RESOLVER_ANSWER                   1       // struct necp_client_host_resolver_answer
#define NECP_CLIENT_SIGN_TYPE_BROWSE_RESULT                     2       // struct necp_client_browse_result
#define NECP_CLIENT_SIGN_TYPE_SERVICE_RESOLVER_ANSWER           3       // struct necp_client_service_resolver_answer

// These sign types are valid for system-wide use.
// The client_id UUID is a unique per-query UUID.
// These should be generated only on behalf of trusted or entitled processes.
#define NECP_CLIENT_SIGN_TYPE_SYSTEM_RESOLVER_ANSWER            4       // struct necp_client_host_resolver_answer
#define NECP_CLIENT_SIGN_TYPE_SYSTEM_BROWSE_RESULT              5       // struct necp_client_browse_result
#define NECP_CLIENT_SIGN_TYPE_SYSTEM_SERVICE_RESOLVER_ANSWER    6       // struct necp_client_service_resolver_answer

struct necp_client_signature {
	u_int8_t signed_tag[32];
} __attribute__((__packed__));

//
struct necp_client_signable {
	uuid_t client_id; // Interpretation depends on sign_type
	u_int32_t sign_type;
	u_int8_t signable_data[0];
} __attribute__((__packed__));

struct necp_client_resolver_answer {
	uuid_t client_id;
	u_int32_t sign_type;
	union sockaddr_in_4_6 address_answer; // Can include port
	u_int32_t hostname_length;
	char hostname[0];
} __attribute__((__packed__));

struct necp_client_host_resolver_answer {
	struct necp_client_signable header;
	u_int32_t metadata_hash; // Extra metadata that can be signed along with the answer, such as a network signature.
	union sockaddr_in_4_6 address_answer; // Can include port
	u_int32_t hostname_length;
	char hostname[0];
} __attribute__((__packed__));

struct necp_client_browse_result {
	struct necp_client_signable header;
	u_int32_t metadata_hash; // Extra metadata that can be signed along with the service, which may include a TXT record or a network signature
	u_int16_t service_length;
	char service[0];
} __attribute__((__packed__));

struct necp_client_service_resolver_answer {
	struct necp_client_signable header;
	u_int32_t metadata_hash; // Extra metadata that can be signed along with the service, which may include a TXT record or a network signature
	u_int16_t service_length;
	u_int16_t port;
	u_int16_t hostname_length;
	char service[0];
	char hostname[0];
} __attribute__((__packed__));

struct necp_client_validatable {
	struct necp_client_signature signature;
	struct necp_client_signable signable;
} __attribute__((__packed__));

#define NECP_CLIENT_ACTION_SIGN_MAX_STRING_LENGTH 1024

#define NECP_CLIENT_ACTION_SIGN_TAG_LENGTH 32

#define NECP_CLIENT_ACTION_SIGN_DEFAULT_DATA_LENGTH 128
#define NECP_CLIENT_ACTION_SIGN_MAX_TOTAL_LENGTH 4096

#define NECP_FILTER_UNIT_NO_FILTER              UINT32_MAX // Reserved filter unit value that prohibits all filters and socket filters

// Type of signed client ID requests.
#define NECP_CLIENT_SIGNED_CLIENT_ID_TYPE_UUID                   1       // struct necp_client_signed_client_id_uuid

struct necp_client_signed_client_id_uuid {
	uuid_t client_id;
	u_int32_t signature_length;
	u_int8_t signature_data[NECP_CLIENT_ACTION_SIGN_TAG_LENGTH];
} __attribute__((__packed__));

/*
 * The sysctl "net.necp.necp_drop_dest_level" controls the global drop rule policy for
 * a set of destinations addresses at the given level -- the drop rule is the last one
 * to be evaluated at this level.
 */
#define MAX_NECP_DROP_DEST_LEVEL_ADDRS 8

struct necp_drop_dest_entry {
	u_int32_t                           level;          // priority level
	u_int32_t                           order;          // session order (read only via sysctl)
	struct necp_policy_condition_addr   cond_addr;
};

struct necp_drop_dest_policy {
	u_int32_t entry_count;
	struct necp_drop_dest_entry entries[MAX_NECP_DROP_DEST_LEVEL_ADDRS];
};


#define NECP_DEMUX_MAX_LEN  32
struct necp_demux_pattern {
	uint16_t offset;
	uint16_t len;
	uint8_t mask[NECP_DEMUX_MAX_LEN];
	uint8_t value[NECP_DEMUX_MAX_LEN];
};

#ifdef BSD_KERNEL_PRIVATE
#include <stdbool.h>
#include <sys/socketvar.h>
#include <sys/kern_control.h>
#include <netinet/ip_var.h>
#include <netinet6/ip6_var.h>
#include <net/if_var.h>
#include <sys/syslog.h>
#include <net/network_agent.h>
#include <net/ethernet.h>
#include <os/log.h>
#if SKYWALK
#include <skywalk/namespace/netns.h>
#endif /* SKYWALK */


SYSCTL_DECL(_net_necp);

extern os_log_t necp_log_handle;
extern os_log_t necp_data_trace_log_handle;

#define NECPLOG(level, format, ...) do {                                                                                        \
	if (level == LOG_ERR) {                                                          \
	    os_log_error(necp_log_handle, "%s: " format "\n", __FUNCTION__, __VA_ARGS__); \
	} else {                                                                         \
	    os_log(necp_log_handle, "%s: " format "\n", __FUNCTION__, __VA_ARGS__);       \
	}                                                                                \
} while (0)

#define NECPLOG0(level, msg) do {                                                                                       \
	if (level == LOG_ERR) {                                                          \
	    os_log_error(necp_log_handle, "%s: %s\n", __FUNCTION__, msg);                 \
	} else {                                                                         \
	    os_log(necp_log_handle, "%s: %s\n", __FUNCTION__, msg);                       \
	}                                                                                \
} while (0)

#define NECPDATATRACELOG(level, format, ...) do {                                                                                        \
    if (level == LOG_ERR) {                                                          \
	os_log_error(necp_data_trace_log_handle, "%s: " format "\n", __FUNCTION__, __VA_ARGS__); \
    } else {                                                                         \
	os_log(necp_data_trace_log_handle, "%s: " format "\n", __FUNCTION__, __VA_ARGS__);       \
    }                                                                                \
} while (0)

enum necp_fd_type_t {
	necp_fd_type_invalid = 0,
	necp_fd_type_session = 1,
	necp_fd_type_client = 2,
};

union necp_sockaddr_union {
	struct sockaddr                 sa;
	struct sockaddr_in              sin;
	struct sockaddr_in6             sin6;
};

/*
 * kstats
 * The ustats and kstats region are mirrored. So when we allocate with
 * skmem_cache from kstats region, we also get an ustats object. To tie them
 * together, kstats has an extra *necp_stats_ustats pointer pointing to the
 * ustats object
 */
struct necp_all_kstats {
	struct necp_all_stats           necp_stats_comm;        /* kernel private stats snapshot */
	struct necp_all_stats           *necp_stats_ustats;     /* points to user-visible stats (in shared ustats region) */
};

extern void necp_client_init(void);
extern int necp_application_find_policy_match_internal(proc_t proc, u_int8_t *parameters __sized_by(parameters_size), u_int32_t parameters_size,
    struct necp_aggregate_result *returned_result,
    u_int32_t *flags, u_int32_t *reason, u_int required_interface_index,
    const union necp_sockaddr_union *override_local_addr,
    const union necp_sockaddr_union *override_remote_addr,
    struct necp_client_endpoint *returned_v4_gateway,
    struct necp_client_endpoint *returned_v6_gateway,
    struct rtentry **returned_route, bool ignore_address,
    bool has_client,
    uuid_t *returned_override_euuid);
/*
 * TLV utilities
 *
 * Note that these functions (other than necp_buffer_find_tlv) do not check the length of the entire buffer,
 * so the caller must be sure that the entire TLV is within bounds.
 */
struct necp_tlv_header {
	u_int8_t type;
	u_int32_t length;
} __attribute__((__packed__));

extern u_int8_t * __counted_by(0) necp_buffer_write_tlv(u_int8_t * __counted_by(0)cursor_, u_int8_t type, u_int32_t length, const void *value __sized_by(length), u_int8_t * buffer __sized_by(buffer_length), u_int32_t buffer_length);
extern u_int8_t * __counted_by(0) necp_buffer_write_tlv_if_different(u_int8_t * __counted_by(0)cursor_, u_int8_t type,
    u_int32_t length, const void *value __sized_by(length), bool *updated,
    u_int8_t * buffer __sized_by(buffer_length), u_int32_t buffer_length);
extern u_int8_t necp_buffer_get_tlv_type(u_int8_t * __counted_by(buffer_length)buffer, size_t buffer_length, u_int32_t tlv_offset);
extern u_int32_t necp_buffer_get_tlv_length(u_int8_t * __counted_by(buffer_length)buffer, size_t buffer_length, u_int32_t tlv_offset);
extern u_int8_t *__sized_by(*value_size) __necp_buffer_get_tlv_value(u_int8_t * __counted_by(buffer_length)buffer, size_t buffer_length, u_int32_t tlv_offset, u_int32_t * value_size);
extern int necp_buffer_find_tlv(u_int8_t * buffer __sized_by(buffer_length), u_int32_t buffer_length, int offset, u_int8_t type, int *err, int next);

/*
 * Inline shim to safely convert the result of `__necp_buffer_get_tlv_value'
 * to an indexable pointer, when appropriate.
 */
__attribute__((always_inline))
static inline u_int8_t * __header_indexable
necp_buffer_get_tlv_value(u_int8_t * __counted_by(buffer_length)buffer, size_t buffer_length, u_int32_t tlv_offset, u_int32_t *value_size)
{
	u_int32_t ensured_value_size = 0;
	u_int8_t *value = __necp_buffer_get_tlv_value(buffer, buffer_length, tlv_offset, &ensured_value_size);
	if (value_size) {
		*value_size = ensured_value_size;
	}
	return value;
}

#define NECPCTL_DROP_ALL_LEVEL                          1       /* Drop all packets if no policy matches above this level */
#define NECPCTL_DEBUG                                           2       /* Log all kernel policy matches */
#define NECPCTL_PASS_LOOPBACK                           3       /* Pass all loopback traffic */
#define NECPCTL_PASS_KEEPALIVES                         4       /* Pass all kernel-generated keepalive traffic */
#define NECPCTL_SOCKET_POLICY_COUNT                     5       /* Count of all socket-level policies */
#define NECPCTL_SOCKET_NON_APP_POLICY_COUNT     6       /* Count of non-per-app socket-level policies */
#define NECPCTL_IP_POLICY_COUNT                         7       /* Count of all ip-level policies */
#define NECPCTL_SESSION_COUNT                           8       /* Count of NECP sessions */
#define NECPCTL_CLIENT_FD_COUNT                         9       /* Count of NECP client fds */
#define NECPCTL_CLIENT_COUNT                            10      /* Count of NECP clients */
#define NECPCTL_ARENA_COUNT                                     11      /* Count of NECP arenas (stats, etc) */
#define NECPCTL_NEXUS_FLOW_COUNT                        12      /* Count of NECP nexus flows */
#define NECPCTL_SOCKET_FLOW_COUNT                       13      /* Count of NECP socket flows */
#define NECPCTL_IF_FLOW_COUNT                           14      /* Count of NECP socket flows */
#define NECPCTL_OBSERVER_FD_COUNT                       15      /* Count of NECP observer fds */
#define NECPCTL_OBSERVER_MESSAGE_LIMIT          16      /* Number of of NECP observer messages allowed to be queued */
#define NECPCTL_SYSCTL_ARENA_COUNT                      17      /* Count of sysctl arenas */
#define NECPCTL_DROP_UNENTITLED_LEVEL                   18      /* Drop unentitled process traffic above this level */
#define NECPCTL_PASS_INTERPOSE                          19      /* Pass interpose */
#define NECPCTL_RESTRICT_MULTICAST                      20      /* Restrict multicast access */
#define NECPCTL_DEDUP_POLICIES                          21      /* Dedup overlapping policies */
#define NECPCTL_CLIENT_TRACING_LEVEL                    22      /* Client tracing level */
#define NECPCTL_CLIENT_TRACING_PID                      23      /* Apply client tracing only to specified pid */
#define NECPCTL_DROP_MANAGEMENT_LEVEL                   24      /* Drop management traffic at this level */

#define NECP_LOOPBACK_PASS_ALL         1  // Pass all loopback traffic
#define NECP_LOOPBACK_PASS_WITH_FILTER 2  // Pass all loopback traffic, but activate content filter and/or flow divert if applicable

#define NECPCTL_NAMES {                                 \
	{ 0, 0 },                                                       \
	{ "drop_all_level", CTLTYPE_INT },      \
	{ "debug", CTLTYPE_INT },                       \
	{ "pass_loopback", CTLTYPE_INT },       \
    { "pass_keepalives", CTLTYPE_INT },     \
    { "pass_interpose", CTLTYPE_INT },      \
}

typedef u_int32_t necp_kernel_policy_id;
#define NECP_KERNEL_POLICY_ID_NONE                      0
#define NECP_KERNEL_POLICY_ID_NO_MATCH          1
#define NECP_KERNEL_POLICY_ID_FIRST_VALID_SOCKET        2
#define NECP_KERNEL_POLICY_ID_FIRST_VALID_IP            UINT16_MAX

typedef u_int32_t necp_app_id;

#define NECP_KERNEL_POLICY_RESULT_NONE                                  0
#define NECP_KERNEL_POLICY_RESULT_PASS                                  NECP_POLICY_RESULT_PASS
#define NECP_KERNEL_POLICY_RESULT_SKIP                                  NECP_POLICY_RESULT_SKIP
#define NECP_KERNEL_POLICY_RESULT_DROP                                  NECP_POLICY_RESULT_DROP
#define NECP_KERNEL_POLICY_RESULT_SOCKET_DIVERT                 NECP_POLICY_RESULT_SOCKET_DIVERT
#define NECP_KERNEL_POLICY_RESULT_SOCKET_FILTER                 NECP_POLICY_RESULT_SOCKET_FILTER
#define NECP_KERNEL_POLICY_RESULT_IP_TUNNEL                             NECP_POLICY_RESULT_IP_TUNNEL
#define NECP_KERNEL_POLICY_RESULT_IP_FILTER                             NECP_POLICY_RESULT_IP_FILTER
#define NECP_KERNEL_POLICY_RESULT_SOCKET_SCOPED                 NECP_POLICY_RESULT_SOCKET_SCOPED
#define NECP_KERNEL_POLICY_RESULT_ROUTE_RULES                   NECP_POLICY_RESULT_ROUTE_RULES
#define NECP_KERNEL_POLICY_RESULT_USE_NETAGENT                  NECP_POLICY_RESULT_USE_NETAGENT
#define NECP_KERNEL_POLICY_RESULT_NETAGENT_SCOPED               NECP_POLICY_RESULT_NETAGENT_SCOPED
#define NECP_KERNEL_POLICY_RESULT_SCOPED_DIRECT                 NECP_POLICY_RESULT_SCOPED_DIRECT
#define NECP_KERNEL_POLICY_RESULT_ALLOW_UNENTITLED              NECP_POLICY_RESULT_ALLOW_UNENTITLED
#define NECP_KERNEL_POLICY_RESULT_REMOVE_NETAGENT               NECP_POLICY_RESULT_REMOVE_NETAGENT

#define NECP_KERNEL_POLICY_PASS_NO_SKIP_IPSEC                   NECP_POLICY_PASS_NO_SKIP_IPSEC
#define NECP_KERNEL_POLICY_PASS_PF_TAG                          NECP_POLICY_PASS_PF_TAG

#define NECP_KERNEL_POLICY_DROP_FLAG_LOCAL_NETWORK              NECP_POLICY_DROP_FLAG_LOCAL_NETWORK
#define NECP_KERNEL_POLICY_DROP_FLAG_SUPPRESS_ALERTS            NECP_POLICY_DROP_FLAG_SUPPRESS_ALERTS

typedef struct {
	u_int32_t identifier;
	u_int32_t data;
} necp_kernel_policy_service;

typedef union {
	u_int                                           tunnel_interface_index;
	u_int                                           scoped_interface_index;
	u_int32_t                                       flow_divert_control_unit;
	u_int32_t                                       filter_control_unit;
	u_int32_t                                       skip_policy_order;
	u_int32_t                                       route_rule_id;
	u_int32_t                                       netagent_id;
	u_int32_t                                       pass_flags;
	u_int32_t                                       drop_flags;
	necp_kernel_policy_service      service;
} necp_kernel_policy_result_parameter;

enum necp_boolean_state {
	necp_boolean_state_unknown = 0,
	necp_boolean_state_false = 1,
	necp_boolean_state_true = 2,
};

struct necp_kernel_socket_policy {
	LIST_ENTRY(necp_kernel_socket_policy)   chain;
	necp_kernel_policy_id           id;
	necp_policy_order                       order;
	u_int32_t                                       session_order;
	int                                                     session_pid;

	u_int64_t                                       condition_mask;
	u_int64_t                                       condition_negated_mask;
	u_int32_t                                       cond_client_flags;
	necp_kernel_policy_id           cond_policy_id;
	u_int32_t                                       cond_app_id;                                    // Locally assigned ID value stored
	u_int32_t                                       cond_real_app_id;                               // Locally assigned ID value stored
	char                                            *cond_custom_entitlement __null_terminated;     // String
	u_int32_t                                       cond_account_id;                                // Locally assigned ID value stored
	char                                            *cond_domain __null_terminated;                 // String
	u_int8_t                                        cond_domain_dot_count;                  // Number of dots in cond_domain
	u_int32_t                                       cond_domain_filter;
	pid_t                                           cond_pid;
	uid_t                                           cond_uid;
	uid_t                                           cond_real_uid;
	ifnet_t                                         cond_bound_interface;                   // Matches specific binding only
	struct necp_policy_condition_tc_range cond_traffic_class;       // Matches traffic class in range
	u_int16_t                                       cond_protocol;                                  // Matches IP protcol number
	union necp_sockaddr_union       cond_local_start;                               // Matches local IP address (or start)
	union necp_sockaddr_union       cond_local_end;                                 // Matches IP address range
	u_int8_t                                        cond_local_prefix;                              // Defines subnet
	union necp_sockaddr_union       cond_remote_start;                              // Matches remote IP address (or start)
	union necp_sockaddr_union       cond_remote_end;                                // Matches IP address range
	u_int8_t                                        cond_remote_prefix;                             // Defines subnet
	struct necp_policy_condition_agent_type cond_agent_type;
	struct necp_policy_condition_sdk_version cond_sdk_version;
	char                                            *cond_signing_identifier __null_terminated;   // String
	char                                            *cond_url __null_terminated;// String
	u_int16_t                                       cond_packet_filter_tags;
	u_int16_t                                       cond_scheme_port;
	int32_t                                         cond_pid_version;
	u_int32_t                                       cond_bound_interface_flags;
	u_int32_t                                       cond_bound_interface_eflags;
	u_int32_t                                       cond_bound_interface_xflags;
	u_int8_t                                        cond_local_networks_flags;

	necp_kernel_policy_result       result;
	necp_kernel_policy_result_parameter     result_parameter;
};

struct necp_kernel_ip_output_policy {
	LIST_ENTRY(necp_kernel_ip_output_policy)        chain;
	necp_kernel_policy_id           id;
	necp_policy_order                       suborder;
	necp_policy_order                       order;
	u_int32_t                                       session_order;
	int                                                     session_pid;

	u_int64_t                                       condition_mask;
	u_int64_t                                       condition_negated_mask;
	necp_kernel_policy_id           cond_policy_id;
	ifnet_t                                         cond_bound_interface;                   // Matches specific binding only
	u_int16_t                                       cond_protocol;                                  // Matches IP protcol number
	union necp_sockaddr_union       cond_local_start;                               // Matches local IP address (or start)
	union necp_sockaddr_union       cond_local_end;                                 // Matches IP address range
	u_int8_t                                        cond_local_prefix;                              // Defines subnet
	union necp_sockaddr_union       cond_remote_start;                              // Matches remote IP address (or start)
	union necp_sockaddr_union       cond_remote_end;                                // Matches IP address range
	u_int8_t                                        cond_remote_prefix;                             // Defines subnet
	u_int32_t                                       cond_last_interface_index;
	u_int16_t                       cond_packet_filter_tags;
	u_int16_t                       cond_scheme_port;
	u_int32_t                                       cond_bound_interface_flags;
	u_int32_t                                       cond_bound_interface_eflags;
	u_int32_t                                       cond_bound_interface_xflags;
	u_int8_t                                        cond_local_networks_flags;

	necp_kernel_policy_result       result;
	necp_kernel_policy_result_parameter     result_parameter;
};

#define MAX_KERNEL_SOCKET_POLICIES                      1
#define MAX_KERNEL_IP_OUTPUT_POLICIES           4
struct necp_session_policy {
	LIST_ENTRY(necp_session_policy) chain;
	bool                            applied;                        // Applied into the kernel table
	bool                            pending_deletion;       // Waiting to be removed from kernel table
	bool                            pending_update;         // Policy has been modified since creation/last application
	necp_policy_id          local_id;
	necp_policy_order       order;
	u_int32_t                       result_size;
	u_int8_t                        *result __sized_by(result_size);
	u_int32_t                       conditions_size;
	u_int8_t                        *conditions __sized_by(conditions_size); // Array of conditions, each with a u_int32_t length at start
	u_int32_t                       route_rules_size;
	u_int8_t                        *route_rules __sized_by(route_rules_size); // Array of route rules, each with a u_int32_t length at start

	uuid_t                          applied_app_uuid;
	uuid_t                          applied_real_app_uuid;
	u_int32_t                       applied_account_size;
	char                            *applied_account __sized_by(applied_account_size);

	uuid_t                          applied_result_uuid;

	u_int32_t                       applied_route_rules_id;

	necp_kernel_policy_id   kernel_socket_policies[MAX_KERNEL_SOCKET_POLICIES];
	necp_kernel_policy_id   kernel_ip_output_policies[MAX_KERNEL_IP_OUTPUT_POLICIES];
};

struct necp_aggregate_socket_result {
	necp_kernel_policy_result                       result;
	necp_kernel_policy_result_parameter     result_parameter;
	necp_kernel_policy_filter                       filter_control_unit;
	u_int32_t                                       flow_divert_aggregate_unit;
	u_int32_t                                                       route_rule_id;
	int32_t                                         qos_marking_gencount;
};

struct necp_inpcb_result {
	u_int32_t                                       app_id;
	necp_kernel_policy_id                           policy_id;
	necp_kernel_policy_id                           skip_policy_id;
	int32_t                                         policy_gencount;
	u_int32_t                                       flowhash;
	u_int32_t                                       network_denied_notifies;// Notification count
	struct necp_aggregate_socket_result             results;
};

extern void necp_init(void);

struct inp_necp_attributes;
extern errno_t necp_set_socket_attributes(struct inp_necp_attributes *attributes, struct sockopt *sopt);
extern errno_t necp_get_socket_attributes(struct inp_necp_attributes *attributes, struct sockopt *sopt);
extern errno_t necp_set_socket_domain_attributes(struct socket *so, const char *domain __null_terminated, const char *domain_owner __null_terminated);
extern int necp_set_socket_resolver_signature(struct inpcb *inp, struct sockopt *sopt);
extern int necp_get_socket_resolver_signature(struct inpcb *inp, struct sockopt *sopt);
extern bool necp_socket_has_resolver_signature(struct inpcb *inp);
extern bool necp_socket_resolver_signature_matches_address(struct inpcb *inp, union necp_sockaddr_union *address);
extern void necp_inpcb_remove_cb(struct inpcb *inp);
extern void necp_inpcb_dispose(struct inpcb *inp);

extern u_int32_t necp_socket_get_content_filter_control_unit(struct socket *so);

extern bool necp_socket_should_use_flow_divert(struct inpcb *inp);
extern u_int32_t necp_socket_get_flow_divert_control_unit(struct inpcb *inp, uint32_t *aggregate_unit);

extern bool necp_socket_should_rescope(struct inpcb *inp);
extern u_int necp_socket_get_rescope_if_index(struct inpcb *inp);
extern u_int32_t necp_socket_get_effective_mtu(struct inpcb *inp, u_int32_t current_mtu);

extern bool necp_socket_is_allowed_to_recv_on_interface(struct inpcb *inp, ifnet_t interface);

extern bool necp_socket_is_allowed_to_send_recv(struct inpcb *inp, ifnet_t input_interface, u_int16_t pf_tag,
    necp_kernel_policy_id *return_policy_id,
    u_int32_t *return_route_rule_id,
    necp_kernel_policy_id *return_skip_policy_id, u_int32_t *return_pass_flags);
extern bool necp_socket_is_allowed_to_send_recv_v4(struct inpcb *inp, u_int16_t local_port,
    u_int16_t remote_port, struct in_addr *local_addr,
    struct in_addr *remote_addr, ifnet_t input_interface, u_int16_t pf_tag,
    necp_kernel_policy_id *return_policy_id, u_int32_t *return_route_rule_id,
    necp_kernel_policy_id *return_skip_policy_id, u_int32_t *return_pass_flags);
extern bool necp_socket_is_allowed_to_send_recv_v6(struct inpcb *inp, u_int16_t local_port,
    u_int16_t remote_port, struct in6_addr *local_addr,
    struct in6_addr *remote_addr, ifnet_t input_interface, u_int16_t pf_tag,
    necp_kernel_policy_id *return_policy_id, u_int32_t *return_route_rule_id,
    necp_kernel_policy_id *return_skip_policy_id, u_int32_t *return_pass_flags);
extern void necp_socket_update_qos_marking(struct inpcb *inp, struct rtentry *route, u_int32_t route_rule_id);
extern bool necp_lookup_current_qos_marking(int32_t *qos_marking_gencount, struct rtentry *route, struct ifnet *interface, u_int32_t route_rule_id, bool old_qos_marking);
extern int necp_mark_packet_from_socket(struct mbuf *packet, struct inpcb *inp, necp_kernel_policy_id policy_id,
    u_int32_t route_rule_id, necp_kernel_policy_id skip_policy_id, u_int32_t pass_flags);
extern necp_kernel_policy_id necp_get_policy_id_from_packet(struct mbuf *packet);
extern necp_kernel_policy_id necp_get_skip_policy_id_from_packet(struct mbuf *packet);
extern u_int16_t necp_get_packet_filter_tags_from_packet(struct mbuf *packet);
extern bool necp_packet_should_skip_filters(struct mbuf *packet);
extern u_int32_t necp_get_last_interface_index_from_packet(struct mbuf *packet);
extern u_int32_t necp_get_route_rule_id_from_packet(struct mbuf *packet);
extern int necp_get_app_uuid_from_packet(struct mbuf *packet,
    uuid_t app_uuid);

extern necp_kernel_policy_id necp_socket_find_policy_match(struct inpcb *inp, struct sockaddr *override_local_addr,
    struct sockaddr *override_remote_addr, u_int32_t override_bound_interface);
extern necp_kernel_policy_id necp_ip_output_find_policy_match(struct mbuf *packet, int flags, struct ip_out_args *ipoa,
    struct rtentry *rt,
    necp_kernel_policy_result *result,
    necp_kernel_policy_result_parameter *result_parameter);
extern necp_kernel_policy_id necp_ip6_output_find_policy_match(struct mbuf *packet, int flags, struct ip6_out_args *ip6oa,
    struct rtentry *rt,
    necp_kernel_policy_result *result,
    necp_kernel_policy_result_parameter *result_parameter);

extern int necp_mark_packet_from_ip(struct mbuf *packet, necp_kernel_policy_id policy_id);
extern int necp_mark_packet_from_ip_with_skip(struct mbuf *packet, necp_kernel_policy_id policy_id, necp_kernel_policy_id skip_policy_id);
extern int necp_mark_packet_from_interface(struct mbuf *packet, ifnet_t interface);

extern ifnet_t necp_get_ifnet_from_result_parameter(necp_kernel_policy_result_parameter *result_parameter);
extern bool necp_packet_can_rebind_to_ifnet(struct mbuf *packet, struct ifnet *interface, struct route *new_route, int family);

extern bool necp_packet_is_allowed_over_interface(struct mbuf *packet, struct ifnet *interface);

extern int necp_mark_packet_as_keepalive(struct mbuf *packet, bool is_keepalive);
extern bool necp_get_is_keepalive_from_packet(struct mbuf *packet);

extern int necp_sign_resolver_answer(uuid_t client_id, u_int32_t sign_type,
    u_int8_t *data, size_t data_length,
    u_int8_t *tag, size_t *out_tag_length);

extern bool necp_validate_resolver_answer(uuid_t client_id, u_int32_t sign_type,
    u_int8_t *data __sized_by(data_length), size_t data_length,
    u_int8_t *tag __sized_by(tag_length), size_t tag_length);

extern int necp_sign_application_id(uuid_t client_id, u_int32_t sign_type,
    u_int8_t *__counted_by(*out_tag_length) tag, size_t *out_tag_length);

extern bool necp_validate_application_id(uuid_t client_id, u_int32_t sign_type,
    u_int8_t *tag __sized_by(tag_length), size_t tag_length);

extern void necp_update_all_clients(void); // Handle general re-evaluate event
extern void necp_update_all_clients_immediately_if_needed(bool should_update_immediately); // Handle general re-evaluate event

extern void necp_force_update_client(uuid_t client_id, uuid_t remove_netagent_uuid, u_int32_t agent_generation); // Cause a single client to get an update event

extern bool necp_set_client_as_background(proc_t proc, struct fileproc *fp, bool background); // Set all clients for an fp as background or not

struct necp_fd_data;
extern void necp_fd_memstatus(proc_t proc, uint32_t status, struct necp_fd_data *client_fd); // Purge memory of clients for the process
extern void necp_fd_defunct(proc_t proc, struct necp_fd_data *client_fd); // Set all clients for an process as defunct

extern void necp_client_request_in_process_flow_divert(pid_t pid);

extern int necp_client_register_socket_flow(pid_t pid, uuid_t client_id, struct inpcb *inp);

extern int necp_client_register_socket_listener(pid_t pid, uuid_t client_id, struct inpcb *inp);

#if SKYWALK
extern int necp_client_get_netns_flow_info(uuid_t client_id, struct ns_flow_info *flow_info);
#endif /* SKYWALK */

extern int necp_client_assert_bb_radio_manager(uuid_t client_id, bool assert);

extern int necp_client_assign_from_socket(pid_t pid, uuid_t client_id, struct inpcb *inp);

extern int necp_assign_client_result(uuid_t netagent_uuid, uuid_t client_id,
    u_int8_t *assigned_results __sized_by(assigned_results_length), size_t assigned_results_length);
extern int necp_assign_client_group_members(uuid_t netagent_uuid, uuid_t client_id,
    u_int8_t *__counted_by(assigned_group_members_length) assigned_group_members,
    size_t assigned_group_members_length);

struct skmem_obj_info;  // forward declaration
extern int necp_stats_ctor(struct skmem_obj_info *oi, struct skmem_obj_info *oim, void *arg, uint32_t skmflag);
extern int necp_stats_dtor(void *addr, void *arg);

/* value to denote invalid flow advisory index */
struct netagent_session;
extern int
necp_update_flow_protoctl_event(uuid_t netagent_uuid, uuid_t client_id,
    uint32_t protoctl_event_code, uint32_t protoctl_event_val,
    uint32_t protoctl_event_tcp_seq_num);

#define NECP_FLOWADV_IDX_INVALID        UINT32_MAX
extern void * __sized_by(*message_length) necp_create_nexus_assign_message(uuid_t nexus_instance, nexus_port_t nexus_port, void *key __sized_by(key_length), uint32_t key_length,
    struct necp_client_endpoint *local_endpoint, struct necp_client_endpoint *remote_endpoint,
    struct ether_addr *local_ether_addr,
    u_int32_t flow_adv_index, void *flow_stats, size_t *message_length);


#define NECP_MAX_DEMUX_PATTERNS 4
struct necp_client_nexus_parameters {
	pid_t pid;
	pid_t epid;
	uuid_t euuid;
#if SKYWALK
	netns_token port_reservation;
#else /* !SKYWALK */
	void *reserved;
#endif /* !SKYWALK */
	union necp_sockaddr_union local_addr;
	union necp_sockaddr_union remote_addr;
	u_int8_t ip_protocol;
	u_int8_t transport_protocol;
	u_int16_t ethertype;
	u_int32_t traffic_class;
	necp_policy_id policy_id;
	necp_policy_id skip_policy_id;
	unsigned is_listener:1;
	unsigned is_interpose:1;
	unsigned is_custom_ether:1;
	unsigned allow_qos_marking:1;
	unsigned override_address_selection:1;
	unsigned use_stable_address:1; // Used if override_address_selection is set
	unsigned no_wake_from_sleep:1;
	unsigned is_demuxable_parent:1;
	unsigned reuse_port:1;

	uuid_t parent_flow_uuid;
	struct necp_demux_pattern demux_patterns[NECP_MAX_DEMUX_PATTERNS];
	uint8_t demux_pattern_count;
};

struct necp_client_group_members {
	size_t group_members_length;
	u_int8_t *group_members __sized_by(group_members_length);
};

struct necp_client_error_parameters {
	int32_t error;
	bool force_report;
};

struct necp_client_agent_parameters {
	union {
		struct necp_client_nexus_parameters nexus_request;
		u_int8_t close_token[QUIC_STATELESS_RESET_TOKEN_SIZE];
		struct necp_client_group_members group_members;
		struct necp_client_error_parameters error;
	} u;
};

#define NECP_CLIENT_CBACTION_NONVIABLE  1
#define NECP_CLIENT_CBACTION_VIABLE     2
#define NECP_CLIENT_CBACTION_INITIAL    3

struct necp_client_add_flow_default {
	uuid_t agent_uuid;
	uuid_t registration_id;
	u_int16_t flags; // NECP_CLIENT_FLOW_FLAGS_*
	u_int16_t stats_request_count;
	struct necp_client_flow_stats stats_requests[1];
} __attribute__((__packed__));

typedef void (*necp_client_flow_cb)(void *handle, int action, uint32_t interface_index, uint32_t necp_flags, bool *viable);

#if SKYWALK
struct skmem_arena_mmap_info;

extern pid_t necp_client_get_proc_pid_from_arena_info(struct skmem_arena_mmap_info *arena_info);

extern void necp_client_early_close(uuid_t client_id); // Cause a single client to close stats, etc

#endif /* SKYWALK */

#endif /* BSD_KERNEL_PRIVATE */

#ifdef KERNEL
#ifdef KERNEL_PRIVATE
struct nstat_domain_info;
extern void necp_copy_inp_domain_info(struct inpcb *, struct socket *, struct nstat_domain_info *);
extern void necp_with_inp_domain_name(struct socket *so, void *ctx, void (*with_func)(char *domain_name __null_terminated, void *ctx));
extern bool net_domain_contains_hostname(char *hostname_string __null_terminated, char *domain_string __null_terminated);
#endif /* KERNEL_PRIVATE */
#endif /* KERNEL */

#ifndef KERNEL

extern int necp_match_policy(const uint8_t *parameters, size_t parameters_size, struct necp_aggregate_result *returned_result);

extern int necp_open(int flags);

extern int necp_client_action(int necp_fd, uint32_t action, uuid_t client_id,
    size_t client_id_len, uint8_t *buffer, size_t buffer_size);

extern int necp_session_open(int flags);

extern int necp_session_action(int necp_fd, uint32_t action,
    uint8_t *in_buffer, size_t in_buffer_length,
    uint8_t *out_buffer, size_t out_buffer_length);

#endif /* !KERNEL */

#endif /* PRIVATE */

#endif
